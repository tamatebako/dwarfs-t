#!/usr/bin/env ruby
# frozen_string_literal: true

require 'fileutils'
require 'json'
require 'net/http'
require 'uri'
require 'time'

##
# Fetcher for Windows CI build logs from GitHub Actions
#
# This script fetches the latest failed Windows Matrix workflow run,
# extracts the CMake build section from the logs, and saves to disk.
#
# Usage:
#   ruby scripts/fetch_windows_ci_logs.rb
#
class WindowsCILogFetcher
  GITHUB_API_BASE = 'https://api.github.com'
  REPO = 'tamatebako/dwarfs'
  WORKFLOW_NAME = 'Windows Matrix.yml'
  OUTPUT_DIR = 'tmp/windows_ci_logs'
  OUTPUT_FILE = File.join(OUTPUT_DIR, 'cmake_build.log')

  # GitHub API with authentication
  def initialize
    @token = ENV.fetch('GITHUB_TOKEN') { `gh auth token`.strip }
    raise 'GITHUB_TOKEN not found' if @token.empty?
  end

  # Main execution
  def run
    puts "🔍 Fetching Windows CI logs..."

    FileUtils.mkdir_p(OUTPUT_DIR)

    run = find_latest_failed_windows_run
    return puts '⏳ No failed Windows build found yet (or still running)' unless run

    puts "📦 Found run #{run['id']}: #{run['status']} - #{run['conclusion']}"

    if run['status'] == 'completed' && run['conclusion'] == 'failure'
      job = find_first_failed_job(run['id'])
      return puts '⏳ Build still running, no failed jobs yet' unless job

      puts "🔧 Fetching logs from job: #{job['name']}"
      logs = fetch_job_logs(job['id'])
      return puts '⏳ Logs not available yet' if logs.nil? || logs.empty?

      cmake_section = extract_cmake_section(logs)

      if cmake_section && !cmake_section.empty?
        save_logs(cmake_section)
        puts "✅ Logs saved to #{OUTPUT_FILE}"
        puts "📊 Last 20 lines:"
        puts cmake_section.lines.last(20).join
      else
        puts '⚠️  No CMake section found in logs'
      end
    elsif run['status'] == 'in_progress' || run['status'] == 'queued'
      puts "⏳ Build still running (status: #{run['status']})"
      puts '   Run the script again later to fetch logs.'
    else
      puts "ℹ️  Latest run status: #{run['status']} - #{run['conclusion']}"
    end
  end

  private

  # Find the latest failed Windows Matrix workflow run
  def find_latest_failed_windows_run
    # First, find the Windows Matrix workflow
    workflows = github_get("/repos/#{REPO}/actions/workflows")
    windows_workflow = workflows['workflows'].find { |w| w['name'] == 'Windows Matrix' || w['path'].include?(WORKFLOW_NAME) }

    return nil unless windows_workflow

    # Get recent runs for this workflow
    runs = github_get("/repos/#{REPO}/actions/workflows/#{windows_workflow['id']}/runs?per_page=10")

    # Find latest failed run (not cancelled)
    runs['workflow_runs'].find do |run|
      run['status'] == 'completed' && run['conclusion'] == 'failure'
    end || runs['workflow_runs'].find { |run| run['status'] == 'in_progress' } # Return in-progress if no failed ones
  end

  # Find the first failed job from a workflow run
  def find_first_failed_job(run_id)
    jobs = github_get("/repos/#{REPO}/actions/runs/#{run_id}/jobs")
    jobs['jobs'].find { |job| job['conclusion'] == 'failure' }
  end

  # Fetch logs for a specific job
  def fetch_job_logs(job_id)
    # The logs endpoint returns a zip file, we need to handle it differently
    # Use gh CLI as fallback since it handles the zip format
    logs = `gh run view #{job_id} --log 2>&1`
    return nil if logs.empty?

    logs
  end

  # Extract the CMake build section from logs
  def extract_cmake_section(logs)
    # First, try to find the structured section
    in_cmake_section = false
    cmake_lines = []
    section_depth = 0

    logs.each_line do |line|
      # Detect start of build section (various possible markers)
      if line =~ /##\[group\].*Build/i || line =~ /🏗️.*Build/i || line.include?('Configure Preset')
        in_cmake_section = true
        section_depth += 1 if line.include?('##[group]')
        cmake_lines << line
        next
      end

      # Track nested groups
      if in_cmake_section
        section_depth += 1 if line.include?('##[group]')
        section_depth -= 1 if line.include?('##[endgroup]')

        # End of section when we return to depth 0 or hit a major section
        if section_depth <= 0 && (line.include?('##[endgroup]') || line =~ /^[🔧📦📊✅❌⏳]/)
          cmake_lines << line if section_depth == 0
          break
        end

        cmake_lines << line
      end
    end

    # If we found content, return it
    return cmake_lines.join if cmake_lines.any? && cmake_lines.size > 10

    # Fallback: just return all lines with errors/warnings
    fallback_lines = []
    logs.each_line do |line|
      fallback_lines << line if line =~ /(error|warning|fatal)\s+[CWE]\d+:/i
    end

    fallback_lines.join
  end

  # Save logs to file with metadata
  def save_logs(content)
    # Also save raw logs for debugging
    raw_file = File.join(OUTPUT_DIR, 'raw_logs.txt')
    File.write(raw_file, content) if content && !content.empty?

    metadata = {
      fetched_at: Time.now.iso8601,
      repo: REPO,
      workflow: WORKFLOW_NAME
    }

    File.write(OUTPUT_FILE, content)
    File.write(File.join(OUTPUT_DIR, 'metadata.json'), JSON.pretty_generate(metadata))
  end

  # Make GitHub API request
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
    puts e.backtrace.first(5).join("\n")
    exit 1
  end
end
