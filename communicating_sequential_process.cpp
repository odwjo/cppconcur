#include <mutex>
#include <condition_variable>
#include <queue>
#include <memory>

//machinery -> logic; logic -> machinery
namespace messaging{
    struct message_base{
        virtual ~message_base(){
        }
    };

    template<typename Msg>
    struct wrapped_message:message_base{
        Msg contents;
        explicit wrapped_message(Msg const& contents_):
            contents(contents_){}
    };
    class queue{
        std::mutex m;
        std::condition_variable c;
        std::queue<std::shared_ptr<message_base>> q;
    public:
        template<typename T>
        void push(T const &msg){
            std::lock_guard<std::mutex> lk(m);
            q.push(std::make_shared<wrapped_message<T>>(msg));
            c.notify_all();
        }
        std::shared_ptr<message_base> wait_and_pop(){
            std::unique_lock<std::mutex> lk(m);
            c.wait(lk,[&]{return !q.empty();});
            auto res = q.front();
            q.pop();
            return res;
        }

    };

    class sender{
        queue *q;
    public:
        sender():q(nullptr){}
        explicit sender(queue *q_):q(q_){}
        template<typename Message>
        void send(Message const& msg){
            if(q){
                q->push(msg);
            }
        }
    };

    class dispatcher;

    class receiver{
        queue q;
    public:
        operator sender(){
            return sender(&q);
        }
        dispatcher wait();
    };

    class close_queue{};

    template<typename Dispatcher,
             typename Msg,
             typename Func>
    class TemplateDispatcher;

    class dispatcher{
        queue *q;
        bool chained;

        dispatcher(dispatcher const&)=delete;
        dispatcher& operator=(dispatcher const&)=delete;

        template<typename Dispatcher,
                 typename Msg,
                 typename Func>
        friend class TemplateDispatcher;

        void wait_and_dispatch(){
            for(;;){
                auto msg = q -> wait_and_pop();
                dispatch(msg);
            }
        }
        bool dispatch(std::shared_ptr<message_base> const& msg){
            if(dynamic_cast<wrapped_message<close_queue>*>(msg.get())){
                throw close_queue();
            }
            return false;
        }
    public:
        dispatcher(dispatcher&& other):
            q(other.q),chained(other.chained){
            other.chained = true;
        }
        explicit dispatcher(queue *q_):
            q(q_), chained(false){}

        template<typename Message, typename Func>
        TemplateDispatcher<dispatcher, Message, Func>
        handle(Func&& f){//construct TemplateDispatcher to deal with it;
            return TemplateDispatcher<dispatcher, Message, Func>(
                        q, this, std::forward<Func>(f));
        }
        ~dispatcher() noexcept(false){
            if(!chained){
                wait_and_dispatch();
            }
        }
    };

    dispatcher receiver::wait(){
        return dispatcher(&q);
    }

    template<typename PreviousDispatcher, typename Msg,
             typename Func>
    class TemplateDispatcher{
        queue *q;
        PreviousDispatcher* prev;
        Func f;
        bool chained;

        TemplateDispatcher(TemplateDispatcher const&)=delete;
        TemplateDispatcher& operator=(TemplateDispatcher const&)=delete;

        template<typename Dispatcher, typename OtherMsg,
                 typename OtherFunc>
        friend class TemplateDispatcher;

        void wait_and_diapatch(){//the msg must be received;
            for(;;){//in case other messages,may be delayed
                auto msg = q->wait_and_pop();
                if(dispatch(msg))//if the msg can't be handle, upto dispatcher will return false;
                    break;
            }
        }
        bool dispatch(std::shared_ptr<message_base> const& msg){
            if(wrapped_message<Msg>* wrapper =
                    dynamic_cast<wrapped_message<Msg>*>(msg.get())){
                f(wrapper -> contents);
                return true;
            }
            else{
                return prev->dispatch(msg);
            }
        }
    public:
        TemplateDispatcher(TemplateDispatcher&& other):
            q(other.q), prev(other.prev),f(std::move(other.f)),
            chained(other.chained){
            other.chained = true;
        }
        TemplateDispatcher(queue *q_,PreviousDispatcher* prev_,Func &&f_):
            q(q_), prev(prev_), f(std::forward<Func>(f_)), chained(false){
            prev_->chained = true;
        }

        template<typename OtherMsg, typename OtherFunc>
        TemplateDispatcher<TemplateDispatcher,OtherMsg,OtherFunc>
        handle(OtherFunc&& of){
            return TemplateDispatcher<
                    TemplateDispatcher,OtherMsg,OtherFunc>(
                        q,this,std::forward<OtherFunc>(of));
        }

        ~TemplateDispatcher() noexcept(false){
            if(!chained){
                wait_and_dispatch();
            }
        }
    };
}


///////ATM messaages//////////
struct withdraw{
    std::string account;
    unsigned amount;
    mutable messaging::sender atm_queue;

    withdraw(std::string const& account_,
             unsigned amount_,
             message::sender atm_queue_):
        account(account_),amount(amount_),
        atm_queue(atm_queue_){}
};

struct withdraw_ok{};

struct withdraw_denied{};

struct cancel_withdrawal{
    std::string account;
    unsigned amount;
    cancel_withdrawal(std::string const& account_,
                      unsigned amount_):
        account(account_),amount(amount_){}
};

struct withdrawal_pocessed{
    std::string account;
    unsigned amount;
    withdrawal_processed(std::string const& account_,
                         unsigned amount_):
        account(account_),amount(amount_){}
};

struct card_inserted{
    std::string account;
    explicit card_inserted(std::string const& account_):
        account(accont_){}
};

struct digit_pressed{
    char digit;
    explicit digit_pressed(char digit_):
        digit(digit_){}
};

struct clear_last_pressed{};

struct eject_card{};

struct withdraw_pressed{
    unsigned amount;
    explicit withdraw_pressed(unsigned amount_):
        amount(amount_){}
};

struct cancel_pressed{};

struct issue_money{
    unsigned amount;
    issue_money(unsigned amount_):
        amount(amount_){}
};

struct verify_pin{
    std::string account;
    std::string pin;
    mutable messaging::sender atm_queue;

    verify_pin(std::string const& account,
        std::string const& pin,
        messaging::sender atm_queue){

    }
};

struct pin_verified{};
struct pin_incorrect{};
struct display_enter_pin{};
struct display_enter_card{};
struct display_insufficient_funds{};
struct display_withdrawal_cancelled{};
struct display_pin_incorrect_message{};
struct display_withdrawal_options{};
struct get_balance{
    std::string account;
    mutable messaging::sender atm_queue;
    get_blance(std::string const& account_,messaging::sender atm_queue_):
        account(account_), atm_queue(atm_queue_){}
};
struct balance{
    unsigned amount;
    explicit balance(unsigned amount_):
        amount(amount_){}
};
struct display_balance{
    unsigned amount;
    explicit display_balance(unsigned amount_):
        amount(amount_){}
};

/////////////ATM state machine//////////
class atm{
    messaging::receiver incoming;
    messaging::sender   bank;
    messaging::sender interface_hardware;
    void(atm::*state)();
    std::string account;
    unsigned withdrawal_amount;

    void process_withdrawal(){
        incoming.wait()
        .handle<withdraw_ok>([&](withdraw_ok const& msg){
            interface_hardware.send(
                issue_money(withdrawal_amount));
            bank.send(
                withdrawal_processed(account,withdrawal_amount));
            state = &atm::done_processing;
        })
        .handle<withdraw_denied>([&](withdraw_denied const& msg){
            interface_hardware.send(
                display_insufficient_funds());
            state = &atm::done_processing;
        })
        .handle<withdraw_pressed>([&](withdraw_ok const& msg){
            bank.send(
                cancel_withdrawal(account,withdrawal_amount));
            interface_hardware.send(
                display_withdrawal_cancelled());
            state = &atm::done_processing;
        });
    }
    void process_balance(){
        incoming.wait()
                .handle<balance>(
                    [&](balance const& msg){
                        interface_hardware.send(display_balance(mag.amount));
                        state = &atm::wait_for_action;
                    }
                    )
                .handle<cancel_pressed>(
                    [&](cancel_pressed const& msg){
                        state = &atm::done_processing;
                    }
                    );
    }
public:
    atm(messaging::sender bank_,
        messaging::sender interface_hardware_):
        bank(bank_),interface_hardware(interface_hardware_){}
    void done(){
        get_sender().send(messaging::close_queue());
    }
    void run(){
        state = &atm::waiting_for_card;
        try{
            for(;;){
                (this->*state)();
            }
        }catch(messaging::close_queue const&){}
    }
    messaging::sender get_sender(){
        return incoming;
    }
};

