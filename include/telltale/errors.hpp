#pragma once

#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <string>

namespace telltale {

enum class ErrorCode {
  Ok = 0,
  IoError,
  InvalidMagic,
  UnsupportedVersion,
  HeaderCrcMismatch,
  RecordCrcMismatch,
  UnexpectedEof,
  PayloadTooLarge,
  InvalidPayload,
  HandlerNotFound,
  HandlerAlreadyRegistered,
  HandlerTableFull,
  InvalidSchemaUpdate,
  InvalidTypeId,
  InvalidHandlerId,
  ChecksumMismatch,
  BatchTooLarge,
  NameTooLong,
  StringTooLong,
  InvalidFlags,
  StateError,
  FinalizeError,
  OpenError,
  SeekError,
  TruncatedRecord,
  EmptyFile,
  DuplicateCounter,
  InvalidBatchStructure,
  MaxRegistrationsExceeded,
  VerifyFailed,
  InternalError
};

class TelltaleError : public std::runtime_error {
 public:
  TelltaleError(ErrorCode code, const std::string& message)
      : std::runtime_error(format_message(code, message)), code_(code) {}

  ErrorCode code() const { return code_; }

  static std::string error_code_name(ErrorCode code) {
    switch (code) {
      case ErrorCode::Ok:
        return "Ok";
      case ErrorCode::IoError:
        return "IoError";
      case ErrorCode::InvalidMagic:
        return "InvalidMagic";
      case ErrorCode::UnsupportedVersion:
        return "UnsupportedVersion";
      case ErrorCode::HeaderCrcMismatch:
        return "HeaderCrcMismatch";
      case ErrorCode::RecordCrcMismatch:
        return "RecordCrcMismatch";
      case ErrorCode::UnexpectedEof:
        return "UnexpectedEof";
      case ErrorCode::PayloadTooLarge:
        return "PayloadTooLarge";
      case ErrorCode::InvalidPayload:
        return "InvalidPayload";
      case ErrorCode::HandlerNotFound:
        return "HandlerNotFound";
      case ErrorCode::HandlerAlreadyRegistered:
        return "HandlerAlreadyRegistered";
      case ErrorCode::HandlerTableFull:
        return "HandlerTableFull";
      case ErrorCode::InvalidSchemaUpdate:
        return "InvalidSchemaUpdate";
      case ErrorCode::InvalidTypeId:
        return "InvalidTypeId";
      case ErrorCode::InvalidHandlerId:
        return "InvalidHandlerId";
      case ErrorCode::ChecksumMismatch:
        return "ChecksumMismatch";
      case ErrorCode::BatchTooLarge:
        return "BatchTooLarge";
      case ErrorCode::NameTooLong:
        return "NameTooLong";
      case ErrorCode::StringTooLong:
        return "StringTooLong";
      case ErrorCode::InvalidFlags:
        return "InvalidFlags";
      case ErrorCode::StateError:
        return "StateError";
      case ErrorCode::FinalizeError:
        return "FinalizeError";
      case ErrorCode::OpenError:
        return "OpenError";
      case ErrorCode::SeekError:
        return "SeekError";
      case ErrorCode::TruncatedRecord:
        return "TruncatedRecord";
      case ErrorCode::EmptyFile:
        return "EmptyFile";
      case ErrorCode::DuplicateCounter:
        return "DuplicateCounter";
      case ErrorCode::InvalidBatchStructure:
        return "InvalidBatchStructure";
      case ErrorCode::MaxRegistrationsExceeded:
        return "MaxRegistrationsExceeded";
      case ErrorCode::VerifyFailed:
        return "VerifyFailed";
      case ErrorCode::InternalError:
        return "InternalError";
      default:
        return "Unknown";
    }
  }

 private:
  ErrorCode code_;

  static std::string format_message(ErrorCode code, const std::string& message) {
    std::ostringstream oss;
    oss << "[" << error_code_name(code) << "] " << message;
    return oss.str();
  }
};

struct Result {
  ErrorCode code;
  std::string message;

  Result() : code(ErrorCode::Ok) {}
  explicit Result(ErrorCode c, const std::string& msg = "") : code(c), message(msg) {}

  bool ok() const { return code == ErrorCode::Ok; }
  explicit operator bool() const { return ok(); }

  static Result success() { return Result(); }

  static Result fail(ErrorCode code, const std::string& message) { return Result(code, message); }
};

}  // namespace telltale
