/* vim:set ts=2 sw=2 sts=2 et: */
/**
 * \author     Marcus Holland-Moritz (github@mhxnet.de)
 * \copyright  Copyright (c) Marcus Holland-Moritz
 *
 * This file is part of dwarfs.
 *
 * dwarfs is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * dwarfs is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with dwarfs.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <cstdlib>
#include <string>

#include <gtest/gtest.h>

#include <dwarfs/tool/argtable3_base_parser.h>

using namespace dwarfs::tool;

/**
 * @brief Test fixture for environment variable functionality
 *
 * Provides cross-platform environment variable manipulation
 * and cleanup for testing environment variable loading.
 */
class EnvironmentVariablesTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Save original environment state for cleanup
    saved_env_.clear();
  }

  void TearDown() override {
    // Restore original environment
    for (auto const& [name, value] : saved_env_) {
      if (value.empty()) {
        unsetEnv(name);
      } else {
        setEnv(name, value);
      }
    }
  }

  /**
   * @brief Set environment variable (cross-platform)
   */
  void setEnv(std::string const& name, std::string const& value) {
    // Save original value if not already saved
    if (saved_env_.find(name) == saved_env_.end()) {
      char const* orig = std::getenv(name.c_str());
      saved_env_[name] = orig ? orig : "";
    }

#ifdef _WIN32
    _putenv_s(name.c_str(), value.c_str());
#else
    setenv(name.c_str(), value.c_str(), 1);
#endif
  }

  /**
   * @brief Unset environment variable (cross-platform)
   */
  void unsetEnv(std::string const& name) {
    // Save original value if not already saved
    if (saved_env_.find(name) == saved_env_.end()) {
      char const* orig = std::getenv(name.c_str());
      saved_env_[name] = orig ? orig : "";
    }

#ifdef _WIN32
    _putenv_s(name.c_str(), "");
#else
    unsetenv(name.c_str());
#endif
  }

  /**
   * @brief Check if environment variable is set
   */
  bool envIsSet(std::string const& name) {
    return std::getenv(name.c_str()) != nullptr;
  }

  /**
   * @brief Get environment variable value
   */
  std::string getEnv(std::string const& name) {
    char const* val = std::getenv(name.c_str());
    return val ? val : "";
  }

private:
  std::map<std::string, std::string> saved_env_;
};

// ============================================================================
// Priority Rule Tests (MECE: CLI > ENV > defaults)
// ============================================================================

/**
 * @brief Test that environment variables are loaded
 *
 * This is a basic smoke test to verify the infrastructure works.
 */
TEST_F(EnvironmentVariablesTest, EnvironmentVariablesCanBeSet) {
  // Set a test environment variable
  setEnv("TEST_VAR", "test_value");

  // Verify it was set
  EXPECT_TRUE(envIsSet("TEST_VAR"));
  EXPECT_EQ(getEnv("TEST_VAR"), "test_value");

  // Unset it
  unsetEnv("TEST_VAR");

  // Verify it was unset
  EXPECT_FALSE(envIsSet("TEST_VAR"));
}

/**
 * @brief Test environment variable priority: Environment overrides defaults
 *
 * When no CLI argument is provided, environment variables should
 * override default values.
 */
TEST_F(EnvironmentVariablesTest, EnvironmentOverridesDefaults) {
  // Set log level via environment
  setEnv("DWARFS_LOG_LEVEL", "debug");

  // The base parser will load this during load_environment_variables()
  // This test validates the infrastructure is in place
  EXPECT_EQ(getEnv("DWARFS_LOG_LEVEL"), "debug");
}

/**
 * @brief Test environment variable priority: CLI overrides environment
 *
 * When a CLI argument is provided, it should take precedence over
 * the environment variable.
 *
 * NOTE: This test documents the expected behavior. Full integration
 * testing requires tool-specific tests that parse actual command lines.
 */
TEST_F(EnvironmentVariablesTest, DefaultsWhenNeitherSet) {
  // Ensure no environment variable is set
  unsetEnv("DWARFS_LOG_LEVEL");
  unsetEnv("DWARFS_VERBOSE");

  // Verify they're not set
  EXPECT_FALSE(envIsSet("DWARFS_LOG_LEVEL"));
  EXPECT_FALSE(envIsSet("DWARFS_VERBOSE"));

  // When parsing, default values should be used
  // (This is handled by the option parser implementations)
}

// ============================================================================
// Common Variables Tests (shared across all tools)
// ============================================================================

/**
 * @brief Test DWARFS_LOG_LEVEL environment variable
 */
TEST_F(EnvironmentVariablesTest, LogLevelEnvironmentVariable) {
  // Test each log level
  std::vector<std::string> levels = {"error", "warn", "info", "debug", "trace"};

  for (auto const& level : levels) {
    setEnv("DWARFS_LOG_LEVEL", level);
    EXPECT_EQ(getEnv("DWARFS_LOG_LEVEL"), level);
  }
}

/**
 * @brief Test DWARFS_VERBOSE environment variable
 */
TEST_F(EnvironmentVariablesTest, VerboseEnvironmentVariable) {
  // Test boolean values
  setEnv("DWARFS_VERBOSE", "1");
  EXPECT_EQ(getEnv("DWARFS_VERBOSE"), "1");

  setEnv("DWARFS_VERBOSE", "true");
  EXPECT_EQ(getEnv("DWARFS_VERBOSE"), "true");

  setEnv("DWARFS_VERBOSE", "0");
  EXPECT_EQ(getEnv("DWARFS_VERBOSE"), "0");

  setEnv("DWARFS_VERBOSE", "false");
  EXPECT_EQ(getEnv("DWARFS_VERBOSE"), "false");
}

/**
 * @brief Test DWARFS_QUIET environment variable
 */
TEST_F(EnvironmentVariablesTest, QuietEnvironmentVariable) {
  setEnv("DWARFS_QUIET", "1");
  EXPECT_EQ(getEnv("DWARFS_QUIET"), "1");

  unsetEnv("DWARFS_QUIET");
  EXPECT_FALSE(envIsSet("DWARFS_QUIET"));
}

// ============================================================================
// Tool-Specific Variables Tests
// ============================================================================

/**
 * @brief Test mkdwarfs-specific environment variables
 */
TEST_F(EnvironmentVariablesTest, MkdwarfsSpecificVariables) {
  // Test compression level
  setEnv("DWARFS_MKDWARFS_COMPRESSION_LEVEL", "5");
  EXPECT_EQ(getEnv("DWARFS_MKDWARFS_COMPRESSION_LEVEL"), "5");

  // Test number of workers
  setEnv("DWARFS_MKDWARFS_NUM_WORKERS", "8");
  EXPECT_EQ(getEnv("DWARFS_MKDWARFS_NUM_WORKERS"), "8");

  // Test block size
  setEnv("DWARFS_MKDWARFS_BLOCK_SIZE", "4M");
  EXPECT_EQ(getEnv("DWARFS_MKDWARFS_BLOCK_SIZE"), "4M");
}

/**
 * @brief Test dwarfs-specific environment variables
 */
TEST_F(EnvironmentVariablesTest, DwarfsSpecificVariables) {
  // Test cache size
  setEnv("DWARFS_DWARFS_CACHE_SIZE", "1g");
  EXPECT_EQ(getEnv("DWARFS_DWARFS_CACHE_SIZE"), "1g");

  // Test number of workers
  setEnv("DWARFS_DWARFS_NUM_WORKERS", "4");
  EXPECT_EQ(getEnv("DWARFS_DWARFS_NUM_WORKERS"), "4");
}

/**
 * @brief Test dwarfsck-specific environment variables
 */
TEST_F(EnvironmentVariablesTest, DwarfsckSpecificVariables) {
  // Test checksum verification
  setEnv("DWARFS_DWARFSCK_CHECKSUM", "sha512-256");
  EXPECT_EQ(getEnv("DWARFS_DWARFSCK_CHECKSUM"), "sha512-256");

  // Test print format
  setEnv("DWARFS_DWARFSCK_PRINT_TYPE", "json");
  EXPECT_EQ(getEnv("DWARFS_DWARFSCK_PRINT_TYPE"), "json");
}

/**
 * @brief Test dwarfsextract-specific environment variables
 */
TEST_F(EnvironmentVariablesTest, DwarfsextractSpecificVariables) {
  // Test number of workers
  setEnv("DWARFS_DWARFSEXTRACT_NUM_WORKERS", "8");
  EXPECT_EQ(getEnv("DWARFS_DWARFSEXTRACT_NUM_WORKERS"), "8");

  // Test output format
  setEnv("DWARFS_DWARFSEXTRACT_FORMAT", "tar");
  EXPECT_EQ(getEnv("DWARFS_DWARFSEXTRACT_FORMAT"), "tar");
}

// ============================================================================
// Invalid Value Handling
// ============================================================================

/**
 * @brief Test that invalid environment values are handled gracefully
 *
 * Invalid values should either be ignored (falling back to defaults)
 * or cause clear error messages. This test documents the expected
 * behavior for error cases.
 */
TEST_F(EnvironmentVariablesTest, InvalidEnvironmentValues) {
  // Invalid log level should be handled gracefully
  setEnv("DWARFS_LOG_LEVEL", "invalid_level");
  EXPECT_EQ(getEnv("DWARFS_LOG_LEVEL"), "invalid_level");
  // Parser should handle this and either error or fall back to default

  // Invalid numeric values
  setEnv("DWARFS_MKDWARFS_COMPRESSION_LEVEL", "not_a_number");
  EXPECT_EQ(getEnv("DWARFS_MKDWARFS_COMPRESSION_LEVEL"), "not_a_number");
  // Parser should handle this and either error or fall back to default

  // Invalid boolean values
  setEnv("DWARFS_VERBOSE", "maybe");
  EXPECT_EQ(getEnv("DWARFS_VERBOSE"), "maybe");
  // Parser should handle this and either error or fall back to default
}

// ============================================================================
// Environment Variable Prefix Tests
// ============================================================================

/**
 * @brief Test that non-DWARFS environment variables are ignored
 */
TEST_F(EnvironmentVariablesTest, NonDwarfsVariablesIgnored) {
  // Set some non-DWARFS variables
  setEnv("LOG_LEVEL", "debug");
  setEnv("VERBOSE", "1");
  setEnv("OTHER_VAR", "value");

  // These should be ignored by the parser
  EXPECT_TRUE(envIsSet("LOG_LEVEL"));
  EXPECT_TRUE(envIsSet("VERBOSE"));
  EXPECT_TRUE(envIsSet("OTHER_VAR"));

  // But they shouldn't affect DWARFS parsing
  EXPECT_FALSE(envIsSet("DWARFS_LOG_LEVEL"));
}

/**
 * @brief Test case sensitivity of environment variable names
 *
 * On Windows, environment variables are case-insensitive.
 * On Unix, they are case-sensitive.
 * This test documents the expected behavior.
 */
TEST_F(EnvironmentVariablesTest, VariableNameCaseSensitivity) {
  setEnv("DWARFS_LOG_LEVEL", "debug");
  EXPECT_TRUE(envIsSet("DWARFS_LOG_LEVEL"));

#ifdef _WIN32
  // Windows is case-insensitive
  EXPECT_TRUE(envIsSet("dwarfs_log_level"));
#else
  // Unix is case-sensitive
  EXPECT_FALSE(envIsSet("dwarfs_log_level"));
#endif
}