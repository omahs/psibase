#![feature(prelude_import)]
#[prelude_import]
use std::prelude::rust_2021::*;
#[macro_use]
extern crate std;
use std::collections::HashMap;
use near_sdk::{
    borsh::{self, BorshDeserialize, BorshSerialize},
    near_bindgen,
};
pub struct MyContract {
    data: HashMap<u64, u64>,
}
#[automatically_derived]
#[allow(unused_qualifications)]
impl ::core::default::Default for MyContract {
    #[inline]
    fn default() -> MyContract {
        MyContract {
            data: ::core::default::Default::default(),
        }
    }
}
impl borsh::ser::BorshSerialize for MyContract
where
    HashMap<u64, u64>: borsh::ser::BorshSerialize,
{
    fn serialize<W: borsh::maybestd::io::Write>(
        &self,
        writer: &mut W,
    ) -> ::core::result::Result<(), borsh::maybestd::io::Error> {
        borsh::BorshSerialize::serialize(&self.data, writer)?;
        Ok(())
    }
}
impl borsh::de::BorshDeserialize for MyContract
where
    HashMap<u64, u64>: borsh::BorshDeserialize,
{
    fn deserialize(buf: &mut &[u8]) -> ::core::result::Result<Self, borsh::maybestd::io::Error> {
        Ok(Self {
            data: borsh::BorshDeserialize::deserialize(buf)?,
        })
    }
}
#[must_use]
pub struct MyContractExt {
    pub(crate) account_id: near_sdk::AccountId,
    pub(crate) deposit: near_sdk::Balance,
    pub(crate) static_gas: near_sdk::Gas,
    pub(crate) gas_weight: near_sdk::GasWeight,
}
impl MyContractExt {
    pub fn with_attached_deposit(mut self, amount: near_sdk::Balance) -> Self {
        self.deposit = amount;
        self
    }
    pub fn with_static_gas(mut self, static_gas: near_sdk::Gas) -> Self {
        self.static_gas = static_gas;
        self
    }
    pub fn with_unused_gas_weight(mut self, gas_weight: u64) -> Self {
        self.gas_weight = near_sdk::GasWeight(gas_weight);
        self
    }
}
impl MyContract {
    /// API for calling this contract's functions in a subsequent execution.
    pub fn ext(account_id: near_sdk::AccountId) -> MyContractExt {
        MyContractExt {
            account_id,
            deposit: 0,
            static_gas: near_sdk::Gas(0),
            gas_weight: near_sdk::GasWeight::default(),
        }
    }
}
impl MyContractExt {
    pub fn insert_data(self, key: u64, value: u64) -> near_sdk::Promise {
        let __args = {
            #[serde(crate = "near_sdk::serde")]
            struct Input<'nearinput> {
                key: &'nearinput u64,
                value: &'nearinput u64,
            }
            #[doc(hidden)]
            #[allow(non_upper_case_globals, unused_attributes, unused_qualifications)]
            const _: () = {
                use near_sdk::serde as _serde;
                #[automatically_derived]
                impl<'nearinput> near_sdk::serde::Serialize for Input<'nearinput> {
                    fn serialize<__S>(
                        &self,
                        __serializer: __S,
                    ) -> near_sdk::serde::__private::Result<__S::Ok, __S::Error>
                    where
                        __S: near_sdk::serde::Serializer,
                    {
                        let mut __serde_state = match _serde::Serializer::serialize_struct(
                            __serializer,
                            "Input",
                            false as usize + 1 + 1,
                        ) {
                            _serde::__private::Ok(__val) => __val,
                            _serde::__private::Err(__err) => {
                                return _serde::__private::Err(__err);
                            }
                        };
                        match _serde::ser::SerializeStruct::serialize_field(
                            &mut __serde_state,
                            "key",
                            &self.key,
                        ) {
                            _serde::__private::Ok(__val) => __val,
                            _serde::__private::Err(__err) => {
                                return _serde::__private::Err(__err);
                            }
                        };
                        match _serde::ser::SerializeStruct::serialize_field(
                            &mut __serde_state,
                            "value",
                            &self.value,
                        ) {
                            _serde::__private::Ok(__val) => __val,
                            _serde::__private::Err(__err) => {
                                return _serde::__private::Err(__err);
                            }
                        };
                        _serde::ser::SerializeStruct::end(__serde_state)
                    }
                }
            };
            let __args = Input {
                key: &key,
                value: &value,
            };
            near_sdk::serde_json::to_vec(&__args)
                .expect("Failed to serialize the cross contract args using JSON.")
        };
        near_sdk::Promise::new(self.account_id).function_call_weight(
            "insert_data".to_string(),
            __args,
            self.deposit,
            self.static_gas,
            self.gas_weight,
        )
    }
    pub fn get_data(self, key: u64) -> near_sdk::Promise {
        let __args = {
            #[serde(crate = "near_sdk::serde")]
            struct Input<'nearinput> {
                key: &'nearinput u64,
            }
            #[doc(hidden)]
            #[allow(non_upper_case_globals, unused_attributes, unused_qualifications)]
            const _: () = {
                use near_sdk::serde as _serde;
                #[automatically_derived]
                impl<'nearinput> near_sdk::serde::Serialize for Input<'nearinput> {
                    fn serialize<__S>(
                        &self,
                        __serializer: __S,
                    ) -> near_sdk::serde::__private::Result<__S::Ok, __S::Error>
                    where
                        __S: near_sdk::serde::Serializer,
                    {
                        let mut __serde_state = match _serde::Serializer::serialize_struct(
                            __serializer,
                            "Input",
                            false as usize + 1,
                        ) {
                            _serde::__private::Ok(__val) => __val,
                            _serde::__private::Err(__err) => {
                                return _serde::__private::Err(__err);
                            }
                        };
                        match _serde::ser::SerializeStruct::serialize_field(
                            &mut __serde_state,
                            "key",
                            &self.key,
                        ) {
                            _serde::__private::Ok(__val) => __val,
                            _serde::__private::Err(__err) => {
                                return _serde::__private::Err(__err);
                            }
                        };
                        _serde::ser::SerializeStruct::end(__serde_state)
                    }
                }
            };
            let __args = Input { key: &key };
            near_sdk::serde_json::to_vec(&__args)
                .expect("Failed to serialize the cross contract args using JSON.")
        };
        near_sdk::Promise::new(self.account_id).function_call_weight(
            "get_data".to_string(),
            __args,
            self.deposit,
            self.static_gas,
            self.gas_weight,
        )
    }
}
impl MyContract {
    pub fn insert_data(&mut self, key: u64, value: u64) -> Option<u64> {
        self.data.insert(key, value)
    }
    pub fn get_data(&self, key: u64) -> Option<u64> {
        self.data.get(&key).cloned()
    }
}
