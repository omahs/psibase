#include <psibase/contract.hpp>
#include <psibase/dispatch.hpp>

// The contract
struct example_contract : psibase::contract
{
   // Add two numbers
   int32_t add(int32_t a, int32_t b) { return a + b; }

   // Multiply two numbers
   int32_t multiply(int32_t a, int32_t b) { return a * b; }
};

// Enable reflection. This enables PSIBASE_DISPATCH and other
// mechanisms to operate.
PSIO_REFLECT(example_contract,  //
             method(add, a, b),
             method(multiply, a, b))

// Allow users to invoke reflected methods inside transactions.
// Also allows other contracts to invoke these methods.
PSIBASE_DISPATCH(example_contract)
