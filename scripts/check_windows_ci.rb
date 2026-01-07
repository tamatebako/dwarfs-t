#!/usr/bin/env ruby
# frozen_string_literal: true

# Simple script to check Windows CI and fetch logs if failed
# Usage: ruby scripts/check_windows_ci.rb

require 'json'

def check_ci
  result = `gh run list --limit 5 --json databaseId,status,conclusion,headSha,displayTitle --jq '.[] | select(.displayTitle | contains("Windows Matrix")) | {databaseId, displayTitle, status, conclusion, headSha}'`

  if result.empty?
    puts "⏳ No Windows Matrix runs found"
    return false
  end

  runs = JSON.parse(result)

  # Find latest run for my commits
  my_runs = runs.select { |r| r['headSha']&.start_with?('86a034b', '707936c') }
  latest_run = my_runs.first || runs.first

  puts "📦 Latest run: #{latest_run['displayTitle']}"
  puts "   Status: #{latest_run['status']}, Conclusion: #{latest_run['conclusion']}"
  puts "   SHA: #{latest_run['headSha'][0..7]}"

  if latest_run['status'] == 'completed' && latest_run['conclusion'] == 'failure'
    puts "❌ Build failed! Fetching logs..."
    system('ruby scripts/fetch_windows_ci_logs_octokit.rb')
    return true
  elsif latest_run['status'] == 'in_progress'
    puts "⏳ Build still running..."
    return false
  elsif latest_run['conclusion'] == 'success'
    puts "✅ Build succeeded!"
    return true
  else
    puts "⚠️  Unknown status: #{latest_run['status']} - #{latest_run['conclusion']}"
    return false
  end
end

if __FILE__ == $PROGRAM_NAME
  check_ci
end
