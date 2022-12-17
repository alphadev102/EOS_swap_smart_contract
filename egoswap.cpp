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
    TABLE config {
        string contract_name;
        string contract_version;
        name admin;
        asset pending_platform_fee;

        // uint64_t total_tasks;
        // uint64_t tasks_completed;

        EOSLIB_SERIALIZE(config, (contract_name)(contract_version)(admin))
    };
    typedef singleton<name("config"), config> config_table;

    struct [[eosio::table]] bot {
      name key;
      bool role;

      uint64_t primary_key() const { return key.value;}
    };
    
    using bot_role = eosio::multi_index<"bot"_n, bot>;

    static constexpr name operate_account=name("opataccount");//change this field
    static constexpr name call_account=name("callaccount");//change this field

    [[eosio::action]]
    void init(string contract_name, string contract_version, name initial_admin, asset main_asset)
    {
        //authenticate
        require_auth(get_self());

        //open config table
        config_table configs(get_self(), get_self().value);

        //validate
        check(!configs.exists(), "config already initialized");
        check(is_account(initial_admin), "initial admin account doesn't exist");

        //initialize
        config new_conf = {
            contract_name, //contract_name
            contract_version, //contract_version
            initial_admin, //admin
            main_asset
        };

        //set new config
        configs.set(new_conf, get_self());
    }

    [[eosio::action]]
    void setadmin(name new_admin)
    {
        //open config table, get config
        config_table configs(get_self(), get_self().value);
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

        check(get_self() == conf.admin, "No authority to set bot");

        bot_role bots(get_self(), get_first_receiver().value);
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
    void withdrawfee(name recipient, asset amount) {
        //open config table, get config
        config_table configs(get_self(), get_self().value);
        auto conf = configs.get();

        check(get_self() == conf.admin, "No authority to withdraw");

        conf.pending_platform_fee -= amount;

        transfer(get_self(),operate_account,recipient,amount,get_self().to_string()+" "+recipient.to_string());
        //transfer(code,operate_account,name("alcorammswap"),quantity,memo);

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

    void exalcor(name code,asset quantity,std::string memo){
      require_auth(operate_account);
      transfer(code,operate_account,name("alcorammswap"),quantity,memo);
    }

    [[eosio::action]]
    void buytoken(asset eos_amount, string target_token, int64_t token_amount_per_native, int64_t slippage_bips, int64_t platform_fee_bips, int64_t gas_estimate, name recipient)
    {
        require_auth(operate_account);

        bot_role bots(get_self(), get_first_receiver().value);
        auto iterator = bots.find(get_self().value);
        check(iterator!= bots.end(), "No exist bot"); // there isn`t bot

        check(iterator->role == true, "No bot role");

        check(slippage_bips <= 10000, "Over Slippage");

        check(gas_estimate < eos_amount.amount, "Insuffcient Token");

        int64_t _eos_amount = eos_amount.amount - gas_estimate;
        int64_t platform_fee = platform_fee_bips * eos_amount.amount / 10000;
        
        int64_t amount_out_min = _eos_amount * token_amount_per_native * (10000 - slippage_bips) / 10000000000;
        _eos_amount -= platform_fee;

        eos_amount.amount = _eos_amount;

        transfer(get_self(),operate_account,name("alcorammswap"), eos_amount, target_token+" "+recipient.to_string());
    }
};