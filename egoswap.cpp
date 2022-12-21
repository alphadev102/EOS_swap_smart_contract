#include <eosio/eosio.hpp>
#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/singleton.hpp>

using namespace std;
using namespace eosio;

class [[eosio::contract]] egoswap : public contract {
  public:
    using contract::contract;

    //contract config
    //scope: self
    //ram payer: self
    // TABLE configs {
    //     name admin;
    //     asset pending_platform_fee;

    //     // uint64_t total_tasks;
    //     // uint64_t tasks_completed;

    //     EOSLIB_SERIALIZE(configs, (admin)(pending_platform_fee))
    // };
    // typedef singleton<name("configs"), configs> config_table;

    // struct [[eosio::table]] bots {
    //   name key;
    //   bool role;

    //   uint64_t primary_key() const { return key.value;}
    // };
    
    // using bot_role = eosio::multi_index<"bots"_n, bots>;

    [[eosio::action]]

    void init(name user, name initial_admin, asset pending_platform_fee)
    {
        //authenticate
        require_auth(get_self());

        //open config table
        config_table config(get_self(), get_self().value);

        //validate
        check(!config.exists(), "config already initialized");
        check(is_account(initial_admin), "initial admin account doesn't exist");

        //initialize
        configs new_conf = {
            initial_admin, //admin
            pending_platform_fee
        };

        //set new config
        config.set(new_conf, get_self());
    }

    [[eosio::action]]
    void setadmin(name new_admin)
    {
        //authenticate
        //require_auth(get_self());

        //open config table, get config
        config_table configs(get_self(), get_self().value);

        check(!config.exists(), "config is not initialized");

        auto conf = configs.get();

        //authenticate
        require_auth(conf.admin);

        //validate
        check(is_account(new_admin), "new admin account doesn't exist");

        //set new admin
        conf.admin = new_admin;

        //update config table
        configs.set(conf, get_self());
    }

    [[eosio::action]]
    void setbotrole(name new_bot, bool brole)
    {
        //open config table, get config
        config_table configs(get_self(), get_self().value);
        auto conf = configs.get();

        // check auth
        require_auth(conf.admin);

        bot_role bots(get_self(), get_self().value);
        auto iterator = bots.find(new_bot.value);
        if( iterator == bots.end() )
        {
          //The user isn't in the table
          bots.emplace(new_bot, [&]( auto& row ) {
            row.key = new_bot;
            row.role = brole;
          });
        }
        else {
          //The user is in the table
          bots.modify(iterator, new_bot, [&]( auto& row ) {
            row.key = new_bot;
            row.role = brole;
          });
        }
    }

    [[eosio::action]]
    void withdrawfee(name user, name recipient, asset amount) {
        
        //open config table, get config
        config_table configs(get_self(), get_self().value);
        auto conf = configs.get();

        // check auth
        require_auth(conf.admin);
        //check(get_self() == conf.admin, "No authority to withdraw");

        check(conf.pending_platform_fee >= amount, "Amount too high");
        conf.pending_platform_fee -= amount;

        transfer("eosio.token"_n, get_self(), recipient, amount, get_self().to_string()+" "+recipient.to_string());

        //update config table
        configs.set(conf, get_self());
    }

    void transfer(name code,name from,name to,asset quantity,std::string memo){
        action(
            permission_level{from, "active"_n},
            name(code), 
            "transfer"_n, 
            std::make_tuple(from,to,quantity,memo)
        ).send();     
    }

    // void exalcor(name code,asset quantity,std::string memo){
    //   require_auth(operate_account);
    //   transfer(code,operate_account,name("alcorammswap"),quantity,memo);
    // }

    [[eosio::action]]
    void buytoken(name user, asset eos_amount, string target_token, asset token_amount_per_native, int64_t slippage_bips, int64_t platform_fee_bips, int64_t gas_estimate, name recipient)
    {
        // check auth
        require_auth(user);

        //require_auth(operate_account);
        config_table configs(get_self(), get_self().value);
        auto conf = configs.get();

        bot_role bots(get_self(), get_self().value);
        auto iterator = bots.find(user);
        check(iterator!= bots.end(), "No exist bot"); // there isn`t bot

        check(iterator->role == true, "No bot role");

        check(slippage_bips <= 10000, "Over Slippage");

        check(gas_estimate < eos_amount.amount, "Insuffcient Token");

        int64_t _eos_amount = eos_amount.amount - gas_estimate;
        int64_t platform_fee = platform_fee_bips * eos_amount.amount / 10000;
        
        int64_t amount_out_min = _eos_amount * token_amount_per_native.amount * (10000 - slippage_bips) / 10000000000;
        _eos_amount -= platform_fee;

        eos_amount.amount = _eos_amount;

        transfer("eosio.token"_n,get_self(),name("alcorammswap"), eos_amount, target_token+" "+recipient.to_string());

        conf.pending_platform_fee.amount += platform_fee;

        configs.set(conf, get_self());
    }
};