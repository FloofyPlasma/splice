// Demonstrates using Head hooks to enforce preconditions and cancel
// method calls that would otherwise cause invalid state.
//
// Rather than adding validation logic directly to the original class,
// hooks let you layer it on externally: keeping the original clean
// and making the validation easy to adjust or remove.

#include <print>
#include <splice/splice.hpp>
#include <string>

class BankAccount
{
public:
  float balance = 1000.0f;

  [[= splice::hook::hookable { }]] void withdraw(float amount)
  {
    balance -= amount;
    std::println("  withdrew {:.2f}, balance is now {:.2f}", amount, balance);
  }

  [[= splice::hook::hookable { }]] void transfer(std::string_view to, float amount)
  {
    balance -= amount;
    std::println("  transferred {:.2f} to {}", amount, to);
  }
};

SPLICE_HOOK_REGISTRY(BankAccount, g_account);

void setupValidation()
{
  // Reject withdrawals of zero or negative amounts.
  g_account->inject<^^BankAccount::withdraw, splice::hook::InjectPoint::Head>(
      [](splice::detail::CallbackInfo &ci, BankAccount *, float amount)
      {
        if (amount <= 0.0f)
        {
          std::println("[validation] rejected withdrawal — amount must be "
                       "positive (got {:.2f})",
              amount);
          ci.cancelled = true;
        }
      },
      splice::hook::Priority::Highest);

  // Reject withdrawals that would overdraw the account.
  g_account->inject<^^BankAccount::withdraw, splice::hook::InjectPoint::Head>(
      [](splice::detail::CallbackInfo &ci, BankAccount *self, float amount)
      {
        if (amount > self->balance)
        {
          std::println("[validation] rejected withdrawal — insufficient funds "
                       "(requested {:.2f}, available {:.2f})",
              amount, self->balance);
          ci.cancelled = true;
        }
      },
      splice::hook::Priority::High);

  // Reject transfers to empty account identifiers.
  g_account->inject<^^BankAccount::transfer, splice::hook::InjectPoint::Head>(
      [](splice::detail::CallbackInfo &ci, BankAccount *, std::string_view to, float)
      {
        if (to.empty())
        {
          std::println("[validation] rejected transfer — destination account is empty");
          ci.cancelled = true;
        }
      },
      splice::hook::Priority::Highest);
}

int main()
{
  setupValidation();

  BankAccount account;

  std::println("--- valid withdrawal ---");
  g_account->dispatch<^^BankAccount::withdraw>(&account, 100.0f);

  std::println("\n--- negative amount ---");
  g_account->dispatch<^^BankAccount::withdraw>(&account, -50.0f);

  std::println("\n--- overdraft ---");
  g_account->dispatch<^^BankAccount::withdraw>(&account, 5000.0f);

  std::println("\n--- valid transfer ---");
  g_account->dispatch<^^BankAccount::transfer>(&account, "acc-002", 200.0f);

  std::println("\n--- empty destination ---");
  g_account->dispatch<^^BankAccount::transfer>(&account, "", 200.0f);
}
