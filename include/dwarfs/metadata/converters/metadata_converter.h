#pragma once

#include <memory>
#include <string_view>

namespace dwarfs::metadata {
  namespace domain { class metadata; }
}

namespace dwarfs::metadata::converters {

/**
 * Interface for converting between format-specific and domain models.
 *
 * Single Responsibility: Define conversion contract
 * Open/Closed: Extensible for new formats without modification
 * Dependency Inversion: Depend on abstractions not concretions
 *
 * This interface follows the Adapter pattern, allowing different
 * format-specific types (Thrift, JSON, etc.) to be converted to/from
 * the pure domain model.
 */
class IMetadataConverter {
public:
  virtual ~IMetadataConverter() = default;

  /**
   * Convert from format-specific model to domain model.
   *
   * @param format_specific Pointer to format-specific metadata
   * @return Domain metadata object
   * @throws std::invalid_argument if format_specific is null
   * @throws std::runtime_error if conversion fails
   */
  virtual domain::metadata to_domain(
      const void* format_specific) const = 0;

  /**
   * Convert from domain model to format-specific model.
   *
   * @param domain_meta Domain metadata object
   * @return Unique pointer to format-specific metadata with custom deleter
   * @throws std::runtime_error if conversion fails
   */
  virtual std::unique_ptr<void, void(*)(void*)> from_domain(
      const domain::metadata& domain_meta) const = 0;

  /**
   * Get the name of this converter (for debugging and logging).
   *
   * @return Human-readable converter name
   */
  virtual std::string_view get_name() const noexcept = 0;
};

} // namespace dwarfs::metadata::converters