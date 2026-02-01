#!/usr/bin/env ruby
# frozen_string_literal: true

require 'fileutils'
require 'json'
require 'optparse'
require 'time'
require 'octokit'
require 'tmpdir'

##
# Fetcher for Windows CI build logs from GitHub Actions using Octokit
#
# This script fetches Windows Matrix workflow run logs,
# downloads the full logs, and organizes them by group for easier inspection.
#
# Usage:
#   ruby scripts/fetch_windows_ci_logs_octokit.rb [options]
#
# Options:
#   --wait, -w              Poll and wait for CI to complete before fetching
#   --poll-interval SECS    Seconds between polls (default: 30)
#   --run-id ID            Fetch logs for specific run ID
#   --latest, -l            Fetch logs for latest run (failed or success)
#   --jobs PATTERN, -j     Filter jobs by name pattern (e.g., "MinGW", "x64")
#   --failures-only, -f    Only fetch logs for failed jobs
#   --summary              Show summary of failed jobs at the end (enabled by default)
#   --no-summary           Disable the failure summary
#
# Examples:
#   # Fetch only MinGW and MSYS job logs
#   ruby scripts/fetch_windows_ci_logs_octokit.rb --jobs MinGW
#
#   # Fetch only failed jobs (saves time and bandwidth)
#   ruby scripts/fetch_windows_ci_logs_octokit.rb --failures-only
#
#   # Show detailed summary of what failed
#   ruby scripts/fetch_windows_ci_logs_octokit.rb --summary
#
class WindowsCILogFetcher
  REPO = 'tamatebako/dwarfs'
  WORKFLOW_NAME = 'Windows Matrix'
  OUTPUT_DIR = 'tmp/windows_ci_logs'
  DEFAULT_POLL_INTERVAL = 30
  MAX_WAIT_TIME = 3600 # 1 hour max wait

  def initialize(options = {})
    @client = Octokit::Client.new(access_token: ENV.fetch('GITHUB_TOKEN') { `gh auth token`.strip })
    @output_run_dir = nil
    @wait = options[:wait]
    @poll_only = options[:poll_only]
    @poll_interval = options[:poll_interval] || DEFAULT_POLL_INTERVAL
    @specific_run_id = options[:run_id]
    @fetch_latest = options[:latest]
    @show_status = options[:show_status]
    @job_filter = options[:job_filter]
    @failures_only = options[:failures_only]
    # Enable summary by default unless explicitly disabled
    @show_summary = options.fetch(:show_summary, true)
    @failed_jobs = []
    @passed_jobs = []
  end

  def run
    if @show_status
      show_current_status
      return
    end

    puts "🔍 Fetching Windows CI logs..."

    FileUtils.mkdir_p(OUTPUT_DIR)

    run = if @specific_run_id
            fetch_specific_run(@specific_run_id)
          elsif @wait || @poll_only
            wait_and_fetch_run
          else
            fetch_latest_run
          end

    return nil unless run

    return run if @poll_only

    process_run(run)
  rescue StandardError => e
    puts "❌ Error: #{e.message}"
    puts e.backtrace.first(10).join("\n")
    exit 1
  end

  private

  def show_current_status
    workflow_id = find_windows_workflow_id
    return unless workflow_id

    puts "📊 Windows CI Status:"
    puts

    # Get recent runs
    runs_result = @client.workflow_runs(REPO, workflow_id, per_page: 5)
    runs_list = runs_result[:workflow_runs] || runs_result['workflow_runs'] || []

    runs_list.each_with_index do |run, idx|
      run_id = run[:id] || run['id']
      status = run[:status] || run['status']
      conclusion = run[:conclusion] || run['conclusion']
      branch = run[:head_branch] || run['head_branch']
      sha = run[:head_sha] || run['head_sha'] || run[:sha] || run['sha']
      title = run[:display_title] || run['display_title'] || run[:name] || run['name']
      created = run[:created_at] || run['created_at']
      updated = run[:updated_at] || run['updated_at']

      status_icon = case status
                    when 'completed' then conclusion == 'success' ? '✅' : '❌'
                    when 'in_progress' then '⏳'
                    when 'queued' then '🔄'
                    else '❓'
                    end

      puts "  #{status_icon} Run #{run_id}: #{status}"
      puts "     Conclusion: #{conclusion || 'N/A'}"
      puts "     Branch: #{branch}"
      puts "     Commit: #{sha[0..7]}"
      puts "     Title: #{title}"
      puts "     Created: #{created}"
      puts "     Updated: #{updated}"
      puts unless idx == runs_list.size - 1
    end
  end

  def fetch_specific_run(run_id)
    puts "📍 Fetching specific run: #{run_id}"
    @client.workflow_run(REPO, run_id)
  rescue Octokit::NotFound
    puts "❌ Run #{run_id} not found"
    exit 1
  end

  def wait_and_fetch_run
    workflow_id = find_windows_workflow_id
    return nil unless workflow_id

    puts "⏳ Waiting for Windows CI to complete..."
    puts "   Polling every #{@poll_interval} seconds (max #{MAX_WAIT_TIME}s)"

    start_time = Time.now
    latest_run = nil

    loop do
      # Check for timeout
      if Time.now - start_time > MAX_WAIT_TIME
        puts "⏱️  Timeout after #{MAX_WAIT_TIME}s"
        return nil
      end

      # Get latest run
      runs_result = @client.workflow_runs(REPO, workflow_id, per_page: 1)
      runs_list = runs_result[:workflow_runs] || runs_result['workflow_runs'] || []
      latest_run = runs_list.first

      unless latest_run
        puts '⚠️  No runs found'
        sleep @poll_interval
        next
      end

      status = latest_run[:status] || latest_run['status']
      conclusion = latest_run[:conclusion] || latest_run['conclusion']
      run_id = latest_run[:id] || latest_run['id']
      branch = latest_run[:head_branch] || latest_run['head_branch']
      sha = latest_run[:head_sha] || latest_run['head_sha'] || latest_run[:sha] || latest_run['sha']
      title = latest_run[:display_title] || latest_run['display_title']

      elapsed = (Time.now - start_time).to_i
      mins = elapsed / 60
      secs = elapsed % 60

      status_icon = case status
                    when 'completed' then conclusion == 'success' ? '✅' : '❌'
                    when 'in_progress' then '⏳'
                    when 'queued' then '🔄'
                    else '❓'
                    end

      if status == 'completed'
        puts "✅ Run #{run_id} completed: #{conclusion}"
        puts "   Branch: #{branch}, Commit: #{sha[0..7]}"
        puts "   Title: #{title[0..80]}..." if title && title.length > 80
        return latest_run
      else
        puts "   #{status_icon} [#{mins}m#{secs}s] Run #{run_id}: #{status} (#{branch} @ #{sha[0..7]})"
      end

      sleep @poll_interval
    end
  end

  def fetch_latest_run
    workflow_id = find_windows_workflow_id
    return nil unless workflow_id

    # Get recent runs
    runs_result = @client.workflow_runs(REPO, workflow_id, per_page: 5)
    runs_list = runs_result[:workflow_runs] || runs_result['workflow_runs'] || []

    # Find latest completed run (success or failure)
    if @fetch_latest
      runs_list.find do |run|
        status = run[:status] || run['status']
        status == 'completed'
      end
    else
      runs_list.find do |run|
        status = run[:status] || run['status']
        conclusion = run[:conclusion] || run['conclusion']
        status == 'completed' && conclusion == 'failure'
      end
    end
  end

  def find_windows_workflow_id
    result = @client.workflows(REPO)

    workflows_list = result[:workflows] || result['workflows'] || []

    windows_workflow = workflows_list.find do |w|
      name = w[:name] || w['name'] || (w.respond_to?(:name) ? w.name : nil)
      name&.include?(WORKFLOW_NAME)
    end

    return nil unless windows_workflow

    windows_workflow[:id] || windows_workflow['id'] || (windows_workflow.respond_to?(:id) ? windows_workflow.id : nil)
  end

  def process_run(run)
    run_id = run[:id] || run['id']
    run_sha = run[:head_sha] || run['head_sha'] || run[:sha] || run['sha']
    run_status = run[:status] || run['status']
    run_conclusion = run[:conclusion] || run['conclusion']

    @output_run_dir = File.join(OUTPUT_DIR, "run_#{run_id}_#{run_sha[0..7]}")
    FileUtils.mkdir_p(@output_run_dir)

    puts "📦 Run #{run_id}: #{run_status} - #{run_conclusion}"
    puts "💾 Output directory: #{@output_run_dir}"

    # Save run metadata
    save_metadata(run)

    if run_status == 'completed'
      # Fetch jobs for this run using the correct API
      jobs_result = @client.workflow_run_jobs(REPO, run_id)

      # workflow_run_jobs returns: {total_count: X, jobs: [...]}
      jobs_list = jobs_result[:jobs] || jobs_result['jobs'] || []

      jobs_list.each do |job|
        process_job(job)
      end

      puts "\n✅ Logs organized in: #{@output_run_dir}"
      list_organized_logs

      show_failure_summary if @show_summary && (@failed_jobs.any? || @passed_jobs.any?)
      @output_run_dir
    else
      puts "⏳ Run still in progress"
      @output_run_dir
    end
  end

  private

  def process_job(job)
    job_name = job[:name] || job.name
    job_conclusion = job[:conclusion] || job.conclusion

    # Track failures and passes
    if job_conclusion == 'failure'
      @failed_jobs << job_name
    elsif job_conclusion == 'success'
      @passed_jobs << job_name
    end

    # Skip if filter doesn't match
    if @job_filter && !job_name.include?(@job_filter)
      puts "  ⊝ #{job_name}: skipped (filter: #{@job_filter})"
      return
    end

    # Skip if failures-only and this job passed
    if @failures_only && job_conclusion == 'success'
      puts "  ✓ #{job_name}: skipped (failures-only)"
      return
    end

    job_dir = File.join(@output_run_dir, sanitize_filename(job_name))
    FileUtils.mkdir_p(job_dir)

    # Save job metadata
    job_metadata = {
      id: job[:id] || job.id,
      name: job[:name] || job.name,
      status: job[:status] || job.status,
      conclusion: job[:conclusion] || job.conclusion,
      started_at: job[:started_at] || job.started_at,
      completed_at: job[:completed_at] || job.completed_at
    }
    File.write(File.join(job_dir, 'metadata.json'), JSON.pretty_generate(job_metadata))

    conclusion = job[:conclusion] || job.conclusion
    if conclusion == 'failure' || conclusion == 'success'
      job_id = job[:id] || job.id
      logs = fetch_job_logs(job_id)
      if logs && !logs.empty?
        organize_logs_by_group(logs, job_dir)
        line_count = logs.lines.count
        puts "  #{conclusion == 'failure' ? '❌' : '✅'} #{job_name}: #{line_count} lines, organized by group"
      else
        puts "  - #{job_name}: no logs available (or still processing)"
      end
    else
      puts "  - #{job_name}: #{conclusion}"
    end
  end

  def fetch_job_logs(job_id)
    # Octokit method for job logs is workflow_run_job_logs
    begin
      logs = @client.workflow_run_job_logs(REPO, job_id)

      # The logs might be:
      # 1. A String with the log content
      # 2. A URL to download the logs
      # 3. A Tempfile object
      # 4. A Sawyer::Resource

      content = nil

      if logs.is_a?(String)
        if logs.start_with?('http')
          # It's a URL, download the content
          content = download_log_url(logs)
        else
          content = logs
        end
      elsif logs.respond_to?(:read)
        content = logs.read
      elsif logs.respond_to?(:to_s)
        str = logs.to_s
        if str.start_with?('http')
          content = download_log_url(str)
        else
          content = str
        end
      end

      content
    rescue Octokit::NotFound
      nil
    rescue Octokit::ServerError => e
      nil
    rescue StandardError => e
      nil
    end
  end

  def download_log_url(url)
    uri = URI.parse(url)
    request = Net::HTTP::Get.new(uri)
    request['Accept'] = 'text/plain'

    response = Net::HTTP.start(uri.hostname, uri.port, use_ssl: uri.scheme == 'https') do |http|
      http.request(request)
    end

    return nil unless response.is_a?(Net::HTTPSuccess)

    response.body
  rescue StandardError
    nil
  end

  def organize_logs_by_group(logs, output_dir)
    current_group = nil
    group_content = []
    groups = {}

    logs.each_line do |line|
      if line =~ /##\[group\](.+)/
        # Save previous group
        if current_group && group_content.any?
          groups[current_group] ||= []
          groups[current_group] << group_content.join
          group_content = []
        end

        # Start new group
        group_title = Regexp.last_match(1).strip
        group_index = groups.keys.grep(/^group_\d+/).size + 1
        current_group = "group_#{group_index.to_s.rjust(2, '0')}_#{sanitize_filename(group_title[0..50])}"
        group_content = [line]
      elsif line =~ /##\[endgroup\]/
        group_content << line
        if current_group && group_content.any?
          groups[current_group] ||= []
          groups[current_group] << group_content.join
          group_content = []
        end
        current_group = nil
      elsif current_group
        group_content << line
      else
        # Lines before first group
        groups['_preamble'] ||= []
        groups['_preamble'] << line
      end
    end

    # Save last group if any
    if current_group && group_content.any?
      groups[current_group] ||= []
      groups[current_group] << group_content.join
    end

    # Write groups to separate files
    groups.each do |group_name, content_parts|
      group_file = File.join(output_dir, "#{group_name}.log")
      full_content = content_parts.join("\n")

      # Limit file size
      if full_content.bytesize > 2_000_000
        # Write sample with truncation notice
        lines = full_content.lines
        header = lines.first(100)
        footer = lines.last(100)
        truncated = "\n\n... [LOG TRUNCATED: #{lines.count - 200} lines omitted] ...\n\n"
        File.write(group_file, header.join + truncated + footer.join)
      else
        File.write(group_file, full_content)
      end
    end

    # Create index
    create_group_index(output_dir, groups)
  end

  def create_group_index(output_dir, groups)
    index_file = File.join(output_dir, '00_index.txt')
    index_lines = ["# Log Groups Index\n\n"]

    groups.each do |name, parts|
      content = parts.join
      size = content.bytesize
      lines = content.lines.count
      index_lines << "- #{name}.log: #{lines} lines, #{format_size(size)}\n"
    end

    File.write(index_file, index_lines.join)
  end

  def list_organized_logs
    puts "\n📁 Organized logs:"
    Dir.glob(File.join(@output_run_dir, '*/00_index.txt')).each do |index_file|
      job_dir = File.dirname(index_file)
      job_name = File.basename(job_dir)
      puts "  #{job_name}/"

      File.readlines(index_file).each do |line|
        next if line =~ /^#/ || line.strip.empty?
        puts "    #{line.strip}"
      end
    end
  end

  def save_metadata(run)
    # Handle Sawyer::Resource which uses hash syntax
    metadata = {
      id: run[:id] || run['id'],
      name: run[:name] || run['name'],
      status: run[:status] || run['status'],
      conclusion: run[:conclusion] || run['conclusion'],
      head_sha: run[:head_sha] || run['head_sha'] || run[:sha] || run['sha'],
      head_branch: run[:head_branch] || run['head_branch'],
      created_at: run[:created_at] || run['created_at'],
      updated_at: run[:updated_at] || run['updated_at'],
      html_url: run[:html_url] || run['html_url'],
      display_title: run[:display_title] || run['display_title'],
      run_number: run[:run_number] || run['run_number'],
      event: run[:event] || run['event'],
      fetched_at: Time.now.iso8601,
      repo: REPO
    }

    File.write(File.join(@output_run_dir, 'run_metadata.json'), JSON.pretty_generate(metadata))
  end

  def sanitize_filename(name)
    name.to_s.gsub(/[^a-zA-Z0-9._-]/, '_')
  end

  def format_size(bytes)
    return '0B' if bytes.zero?

    units = %w[B KB MB GB TB]
    exp = [Math.log(bytes, 1024).to_i, units.size - 1].min
    "#{format('%.1f', bytes.to_f / 1024**exp)}#{units[exp]}"
  end

  def show_failure_summary
    puts "\n" + "="*60
    puts "📊 CI RUN SUMMARY"
    puts "="*60

    if @failed_jobs.any?
      puts "\n❌ FAILED JOBS (#{@failed_jobs.size}):"
      @failed_jobs.each_with_index do |job, i|
        puts "  #{i + 1}. #{job}"
      end
      puts "\n🔧 Next steps:"
      puts "  1. Check the logs for each failed job above"
      puts "  2. Look for error messages in the build logs"
      puts "  3. Fix the issues and push again"
      if @failed_jobs.any? { |j| j.include?('MinGW') || j.include?('MSYS') }
        puts "\n💡 MinGW/MSYS specific tips:"
        puts "   - Check for WinMain/main linking issues"
        puts "   - Verify GCC-style flags (-Dflag not /Dflag)"
        puts "   - Ensure vcpkg triplet is correctly configured"
      end
    end

    if @passed_jobs.any?
      puts "\n✅ PASSED JOBS (#{@passed_jobs.size}):"
      @passed_jobs.each_with_index do |job, i|
        puts "  #{i + 1}. #{job}"
      end
    end

    puts "\n" + "="*60
  end
end

# Parse options
options = {}
parser = OptionParser.new do |opts|
  opts.banner = "Usage: ruby scripts/fetch_windows_ci_logs_octokit.rb [options]"

  opts.on('-w', '--wait', 'Poll and wait for CI to complete before fetching') do
    options[:wait] = true
  end

  opts.on('--poll-only', 'Only poll for status without fetching logs') do
    options[:poll_only] = true
  end

  opts.on('--show-status', 'Show current CI status and exit') do
    options[:show_status] = true
  end

  opts.on('-f', '--failures-only', 'Only fetch logs for failed jobs') do
    options[:failures_only] = true
  end

  opts.on('--[no-]summary', 'Show/hide summary of failed jobs (default: show)') do |val|
    options[:show_summary] = val
  end

  opts.on('--poll-interval SECONDS', Integer, 'Seconds between polls (default: 30)') do |secs|
    options[:poll_interval] = secs
  end

  opts.on('--run-id ID', Integer, 'Fetch logs for specific run ID') do |id|
    options[:run_id] = id
  end

  opts.on('-l', '--latest', 'Fetch logs for latest completed run (success or failure)') do
    options[:latest] = true
  end

  opts.on('-j', '--jobs PATTERN', 'Filter jobs by name pattern (e.g., "MinGW", "x64")') do |pattern|
    options[:job_filter] = pattern
  end

  opts.on('-h', '--help', 'Show this help message') do
    puts opts
    exit
  end
end

parser.parse!

# Run the fetcher
if __FILE__ == $PROGRAM_NAME
  fetcher = WindowsCILogFetcher.new(options)
  result = fetcher.run

  # Output the result path for easy consumption
  if result
    puts result
  end
end
