#pragma once

// TODO: Move this into a common splice aspect
namespace splice::detail
{
  struct fixed_string
  {
    const char *__inner;

    template<typename StringType>
    consteval fixed_string(StringType &some_str)
    {
      this->__inner = std::define_static_string(some_str);
    }

    consteval fixed_string(decltype(nullptr)) { this->__inner = nullptr; }

    operator const char *() { return this->__inner; }

    operator bool() { return this->__inner; }
    bool operator ==(decltype(nullptr)) { return this->__inner == nullptr; }
    bool operator !=(decltype(nullptr)) { return this->__inner != nullptr; }
  };
};

namespace splice::flux
{

  /// @brief Sets the ownership state of a field
  ///
  /// `Own` is the default behavior, which causes the data to be serialized in-place.
  /// If multiple members refer to the same object, they will be serialized as copies
  /// and duplicated on deserialization
  ///
  /// `Ref` marks that the object should be stored in a top-level reference store, and
  /// only a pointer/reference to this stored object should be stored in this field. This
  /// means that multiple references to the same object will result in a single object being
  /// produced on deserialization.
  enum class Ownership
  {
    Own,
    Ref
  };

  /// @brief Marks a struct member for automatic serialization
  ///
  /// Only public non-static members of a struct may be annotated with splice::flux::field
  /// This allows flux to automatically serialize the field for you. The type of the member
  /// should either have a specific specializer for this serialization target
  /// (see: splice::flux::serializer<>), or otherwise be a struct with at least one serializable
  /// field
  struct field
  {
    detail::fixed_string name = nullptr;
    bool replicate = false;
    bool config = false;
    Ownership ownership = Ownership::Own;
  };

  /// @brief Marks a struct for automatic member serialization
  ///
  /// Members of the struct automatically get treated as serializable with the default
  /// values of splice::flux::field. Only public members are treated as serializable.
  struct serializable
  {
  };

} // namespace splice::flux

#define SPLICE_FLUX_FIELD(...)                                                                                         \
  = splice::flux::field { __VA_ARGS__ }

#define SPLICE_FLUX_SERIALIZABLE                                                                                       \
  = splice::flux::serializable { }
