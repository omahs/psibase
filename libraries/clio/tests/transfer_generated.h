// automatically generated by the FlatBuffers compiler, do not modify


#ifndef FLATBUFFERS_GENERATED_TRANSFER_CONTRACT_H_
#define FLATBUFFERS_GENERATED_TRANSFER_CONTRACT_H_

#include "flatbuffers/flatbuffers.h"

namespace contract {

struct transfer;
struct transferBuilder;
struct transferT;

struct transferT : public flatbuffers::NativeTable {
  typedef transfer TableType;
  uint32_t from = 0;
  uint32_t to = 0;
  uint32_t amount = 0;
  std::string memo{};
};

struct transfer FLATBUFFERS_FINAL_CLASS : private flatbuffers::Table {
  typedef transferT NativeTableType;
  typedef transferBuilder Builder;
  enum FlatBuffersVTableOffset FLATBUFFERS_VTABLE_UNDERLYING_TYPE {
    VT_FROM = 4,
    VT_TO = 6,
    VT_AMOUNT = 8,
    VT_MEMO = 10
  };
  uint32_t from() const {
    return GetField<uint32_t>(VT_FROM, 0);
  }
  uint32_t to() const {
    return GetField<uint32_t>(VT_TO, 0);
  }
  uint32_t amount() const {
    return GetField<uint32_t>(VT_AMOUNT, 0);
  }
  const flatbuffers::String *memo() const {
    return GetPointer<const flatbuffers::String *>(VT_MEMO);
  }
  bool Verify(flatbuffers::Verifier &verifier) const {
    return VerifyTableStart(verifier) &&
           VerifyField<uint32_t>(verifier, VT_FROM, 4) &&
           VerifyField<uint32_t>(verifier, VT_TO, 4) &&
           VerifyField<uint32_t>(verifier, VT_AMOUNT, 4) &&
           VerifyOffset(verifier, VT_MEMO) &&
           verifier.VerifyString(memo()) &&
           verifier.EndTable();
  }
  transferT *UnPack(const flatbuffers::resolver_function_t *_resolver = nullptr) const;
  void UnPackTo(transferT *_o, const flatbuffers::resolver_function_t *_resolver = nullptr) const;
  static flatbuffers::Offset<transfer> Pack(flatbuffers::FlatBufferBuilder &_fbb, const transferT* _o, const flatbuffers::rehasher_function_t *_rehasher = nullptr);
};

struct transferBuilder {
  typedef transfer Table;
  flatbuffers::FlatBufferBuilder &fbb_;
  flatbuffers::uoffset_t start_;
  void add_from(uint32_t from) {
    fbb_.AddElement<uint32_t>(transfer::VT_FROM, from, 0);
  }
  void add_to(uint32_t to) {
    fbb_.AddElement<uint32_t>(transfer::VT_TO, to, 0);
  }
  void add_amount(uint32_t amount) {
    fbb_.AddElement<uint32_t>(transfer::VT_AMOUNT, amount, 0);
  }
  void add_memo(flatbuffers::Offset<flatbuffers::String> memo) {
    fbb_.AddOffset(transfer::VT_MEMO, memo);
  }
  explicit transferBuilder(flatbuffers::FlatBufferBuilder &_fbb)
        : fbb_(_fbb) {
    start_ = fbb_.StartTable();
  }
  flatbuffers::Offset<transfer> Finish() {
    const auto end = fbb_.EndTable(start_);
    auto o = flatbuffers::Offset<transfer>(end);
    return o;
  }
};

inline flatbuffers::Offset<transfer> Createtransfer(
    flatbuffers::FlatBufferBuilder &_fbb,
    uint32_t from = 0,
    uint32_t to = 0,
    uint32_t amount = 0,
    flatbuffers::Offset<flatbuffers::String> memo = 0) {
  transferBuilder builder_(_fbb);
  builder_.add_memo(memo);
  builder_.add_amount(amount);
  builder_.add_to(to);
  builder_.add_from(from);
  return builder_.Finish();
}

inline flatbuffers::Offset<transfer> CreatetransferDirect(
    flatbuffers::FlatBufferBuilder &_fbb,
    uint32_t from = 0,
    uint32_t to = 0,
    uint32_t amount = 0,
    const char *memo = nullptr) {
  auto memo__ = memo ? _fbb.CreateString(memo) : 0;
  return contract::Createtransfer(
      _fbb,
      from,
      to,
      amount,
      memo__);
}

flatbuffers::Offset<transfer> Createtransfer(flatbuffers::FlatBufferBuilder &_fbb, const transferT *_o, const flatbuffers::rehasher_function_t *_rehasher = nullptr);

inline transferT *transfer::UnPack(const flatbuffers::resolver_function_t *_resolver) const {
  auto _o = std::unique_ptr<transferT>(new transferT());
  UnPackTo(_o.get(), _resolver);
  return _o.release();
}

inline void transfer::UnPackTo(transferT *_o, const flatbuffers::resolver_function_t *_resolver) const {
  (void)_o;
  (void)_resolver;
  { auto _e = from(); _o->from = _e; }
  { auto _e = to(); _o->to = _e; }
  { auto _e = amount(); _o->amount = _e; }
  { auto _e = memo(); if (_e) _o->memo = _e->str(); }
}

inline flatbuffers::Offset<transfer> transfer::Pack(flatbuffers::FlatBufferBuilder &_fbb, const transferT* _o, const flatbuffers::rehasher_function_t *_rehasher) {
  return Createtransfer(_fbb, _o, _rehasher);
}

inline flatbuffers::Offset<transfer> Createtransfer(flatbuffers::FlatBufferBuilder &_fbb, const transferT *_o, const flatbuffers::rehasher_function_t *_rehasher) {
  (void)_rehasher;
  (void)_o;
  struct _VectorArgs { flatbuffers::FlatBufferBuilder *__fbb; const transferT* __o; const flatbuffers::rehasher_function_t *__rehasher; } _va = { &_fbb, _o, _rehasher}; (void)_va;
  auto _from = _o->from;
  auto _to = _o->to;
  auto _amount = _o->amount;
  auto _memo = _o->memo.empty() ? 0 : _fbb.CreateString(_o->memo);
  return contract::Createtransfer(
      _fbb,
      _from,
      _to,
      _amount,
      _memo);
}

inline const contract::transfer *Gettransfer(const void *buf) {
  return flatbuffers::GetRoot<contract::transfer>(buf);
}

inline const contract::transfer *GetSizePrefixedtransfer(const void *buf) {
  return flatbuffers::GetSizePrefixedRoot<contract::transfer>(buf);
}

inline bool VerifytransferBuffer(
    flatbuffers::Verifier &verifier) {
  return verifier.VerifyBuffer<contract::transfer>(nullptr);
}

inline bool VerifySizePrefixedtransferBuffer(
    flatbuffers::Verifier &verifier) {
  return verifier.VerifySizePrefixedBuffer<contract::transfer>(nullptr);
}

inline void FinishtransferBuffer(
    flatbuffers::FlatBufferBuilder &fbb,
    flatbuffers::Offset<contract::transfer> root) {
  fbb.Finish(root);
}

inline void FinishSizePrefixedtransferBuffer(
    flatbuffers::FlatBufferBuilder &fbb,
    flatbuffers::Offset<contract::transfer> root) {
  fbb.FinishSizePrefixed(root);
}

inline std::unique_ptr<contract::transferT> UnPacktransfer(
    const void *buf,
    const flatbuffers::resolver_function_t *res = nullptr) {
  return std::unique_ptr<contract::transferT>(Gettransfer(buf)->UnPack(res));
}

inline std::unique_ptr<contract::transferT> UnPackSizePrefixedtransfer(
    const void *buf,
    const flatbuffers::resolver_function_t *res = nullptr) {
  return std::unique_ptr<contract::transferT>(GetSizePrefixedtransfer(buf)->UnPack(res));
}

}  // namespace contract

#endif  // FLATBUFFERS_GENERATED_TRANSFER_CONTRACT_H_
