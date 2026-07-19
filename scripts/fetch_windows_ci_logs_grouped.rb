#!/usr/bin/env ruby
# frozen_string_literal: true

require 'fileutils'
require 'json'
require 'net/http'
require 'uri'
require 'time'
require 'zlib'

##
# Fetcher for Windows CI build logs from GitHub Actions
#
# This script fetches the latest failed Windows Matrix workflow run,
# downloads the full logs, and organizes them by group for easier inspection.
#
# Usage:
#   ruby scripts/fetch_windows_ci_logs_grouped.rb
#
class WindowsCILogFetcher
  GITHUB_API_BASE = 'https://api.github.com'
  REPO = 'tamatebako/dwarfs'
  WORKFLOW_NAME = 'Windows Matrix.yml'
  OUTPUT_DIR = 'tmp/windows_ci_logs'
  MAX_LOG_SIZE = 50_000_000 # 50MB max log size

  def initialize
    @token = ENV.fetch('GITHUB_TOKEN') { `gh auth token`.strip }
    raise 'GITHUB_TOKEN not found' if @token.empty?
    @output_run_dir = nil
  end

  def run
    puts "🔍 Fetching Windows CI logs..."

    FileUtils.mkdir_p(OUTPUT_DIR)

    run = find_latest_failed_windows_run
    return puts '⏳ No failed Windows build found yet (or still running)' unless run

    @output_run_dir = File.join(OUTPUT_DIR, "run_#{run['id']}_#{run['head_sha'][0..7]}")
    FileUtils.mkdir_p(@output_run_dir)

    puts "📦 Found run #{run['id']}: #{run['status']} - #{run['conclusion']}"
    puts "💾 Output directory: #{@output_run_dir}"

    # Save run metadata
    save_metadata(run)

    if run['status'] == 'completed' && run['conclusion'] == 'failure'
      jobs = list_jobs(run['id'])

      jobs.each do |job|
        process_job(job)
      end

      puts "\n✅ Logs organized in: #{@output_run_dir}"
      list_organized_logs
    elsif run['status'] == 'in_progress' || run['status'] == 'queued'
      puts "⏳ Build still running (status: #{run['status']})"
      puts '   Run the script again later to fetch logs.'
    else
      puts "ℹ️  Latest run status: #{run['status']} - #{run['conclusion']}"
    end
  end

  private

  def find_latest_failed_windows_run
    workflows = github_get("/repos/#{REPO}/actions/workflows")
    windows_workflow = workflows['workflows'].find { |w| w['name'] == 'Windows Matrix' || w['path'].include?(WORKFLOW_NAME) }
    return nil unless windows_workflow

    runs = github_get("/repos/#{REPO}/actions/workflows/#{windows_workflow['id']}/runs?per_page=5")
    runs['workflow_runs'].find { |run| run['status'] == 'completed' && run['conclusion'] == 'failure' }
  end

  def list_jobs(run_id)
    jobs = github_get("/repos/#{REPO}/actions/runs/#{run_id}/jobs?per_page=100")
    jobs['jobs']
  end

  def process_job(job)
    job_dir = File.join(@output_run_dir, sanitize_filename(job['name']))
    FileUtils.mkdir_p(job_dir)

    # Save job metadata
    File.write(File.join(job_dir, 'metadata.json'), JSON.pretty_generate(job))

    if job['conclusion'] == 'failure' || job['conclusion'] == 'success'
      logs = fetch_job_logs(job['id'])
      if logs && !logs.empty?
        organize_logs_by_group(logs, job_dir)
        puts "  ✓ #{job['name']}: #{logs.lines.count} lines, organized by group"
      else
        puts "  - #{job['name']}: no logs available"
      end
    else
      puts "  - #{job['name']}: #{job['conclusion']}"
    end
  end

  def fetch_job_logs(job_id)
    # Use gh CLI to fetch logs (it handles the zip format)
    logs = `gh run view #{job_id} --log 2>&1`
    return nil if logs.empty? || logs.include?('not found')

    logs
  end

  def organize_logs_by_group(logs, output_dir)
    current_group = nil
    group_content = []
    group_index = 0

    # First pass: collect all groups
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
        current_group = Regexp.last_match(1).strip
        current_group = "group_#{group_index += 1}_#{sanitize_filename(current_group)}"
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

      # If multiple parts, append them
      full_content = content_parts.join("\n")

      # Truncate if too large
      if full_content.bytesize > 1_000_000
        # Write first 500KB and last 500KB with marker
        midpoint = full_content.bytesize / 2
        first_part = full_content.bytes[0...500_000].pack('c*').join
        last_part_start = full_content.bytesize - 500_000
        last_part = full_content.bytes[last_part_start..].pack('c*').join

        File.write(group_file, first_part + "\n\n... [TRUNCATED - #{full_content.bytesize - 1_000_000} bytes omitted] ...\n\n" + last_part)
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
      size = parts.join.bytesize
      lines = parts.join.lines.count
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
    metadata = {
      id: run['id'],
      name: run['name'],
      status: run['status'],
      conclusion: run['conclusion'],
      head_sha: run['head_sha'],
      head_branch: run['head_branch'],
      created_at: run['created_at'],
      updated_at: run['updated_at'],
      url: run['html_url'],
      fetched_at: Time.now.iso8601,
      repo: REPO
    }

    File.write(File.join(@output_run_dir, 'run_metadata.json'), JSON.pretty_generate(metadata))
  end

  def sanitize_filename(name)
    name.gsub(/[^a-zA-Z0-9._-]/, '_')
  end

  def format_size(bytes)
    return '0B' if bytes.zero?

    units = %w[B KB MB GB TB]
    exp = [Math.log(bytes, 1024).to_i, units.size - 1].min
    "#{format('%.1f', bytes.to_f / 1024**exp)}#{units[exp]}"
  end

  def github_get(endpoint)
    uri = URI.join(GITHUB_API_BASE, endpoint)
    request = Net::HTTP::Get.new(uri)
    request['Accept'] = 'application/vnd.github+json'
    request['Authorization'] = "Bearer #{@token}"
    request['X-GitHub-Api-Version'] = '2022-11-28'

    response = Net::HTTP.start(uri.hostname, uri.port, use_ssl: true) do |http|
      http.request(request)
    end

    return nil unless response.is_a?(Net::HTTPSuccess)

    JSON.parse(response.body)
  end
end

# Run the fetcher
if __FILE__ == $PROGRAM_NAME
  begin
    WindowsCILogFetcher.new.run
  rescue StandardError => e
    puts "❌ Error: #{e.message}"
    puts e.backtrace.first(10).join("\n")
    exit 1
  end
end
