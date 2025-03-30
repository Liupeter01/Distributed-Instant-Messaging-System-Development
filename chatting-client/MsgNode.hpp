/* be aware, messagenode.hpp needs c++17 */
#pragma once
#ifndef _MSGNODE_H_
#define _MSGNODE_H_
#include <cstdint>
#include <functional>
#include <iterator>
#include <optional>
#include <string>
#include <type_traits> //SFINAE
#include <utility>     // for std::declval

class QString;
class TCPNetworkConnection;

enum class MsgNodeType{
    MSGNODE_NORMAL,
    MSGNODE_FILE_TRANSFER /*file size no more then 4GB*/
};

template <typename _Ty> struct add_const_lvalue_reference {
    using type = std::add_lvalue_reference_t<std::add_const_t<std::decay_t<_Ty>>>;
};

template <typename _Tp>
using add_const_lvalue_reference_t =
    typename add_const_lvalue_reference<_Tp>::type;

/* usually _Ty should be std::string or Qstring
 * but my point is if this container supports a iterator and size method
 * it should conpatiable with this library
 */
template <typename Container, typename = void>
struct send_msg_check : public std::false_type {};

template <typename Container>
struct send_msg_check<
    Container,
    std::void_t<typename Container::iterator,
                decltype(std::declval<std::decay_t<Container>>().size()),
                decltype(std::declval<std::decay_t<Container>>().begin()),
                decltype(std::declval<std::decay_t<Container>>().end())>>
    : public std::true_type {};

/*creating a SFINAE to test wheather _Ty support append() member function*/
template <typename Container, typename = void>
struct recv_msg_check : public std::false_type {};

/*
 * 1. support begin/end iterator
 * 2. support append function
 *      append could both parameters which have copy ctor and copt ctor disabled
 * functions a) append(Container) b) append(const Container &)
 */
template <typename Container>
struct recv_msg_check<
    Container,
    std::void_t<typename Container::iterator,
                decltype(std::declval<std::decay_t<Container>>().size()),
                decltype(std::declval<std::decay_t<Container>>()
                             .append(/*support copy ctor*/
                                     std::declval<std::decay_t<Container>>())),
                decltype(std::declval<std::decay_t<Container>>()
                             .append(/*copy ctor disabled*/
                                     std::declval<add_const_lvalue_reference_t<
                                         Container>>())),
                decltype(std::declval<std::decay_t<Container>>().data()),
                decltype(std::declval<std::decay_t<Container>>().begin()),
                decltype(std::declval<std::decay_t<Container>>().end())>>
    : public std::true_type {};

template <typename _Ty, typename Callable, typename = void> class RecvNode {
public:
    RecvNode() {
        static_assert(sizeof(_Ty) == 0,
                      "This type does not support append() member function");
    }
};

/*creating a SFINAE to test wheather _Ty support size() member function*/
template <typename _Ty, typename Callable, typename = void> class SendNode {
public:
    SendNode() {
        static_assert(
            sizeof(_Ty) == 0,
            "This type does not support size, begin and end member functions");
    }
};

template <typename Container> struct MsgHeader {
    /*letting tcpnetwork to handle protected _buffer*/
    friend class TCPNetworkConnection;

    /*For receiving data*/
    MsgHeader(MsgNodeType type = MsgNodeType::MSGNODE_NORMAL) noexcept
        : _id(0)
        , _type(type)
        , _cur_length(0)
    {
        _length = get_header_length();
        _buffer.resize(_length, 0);
    }

    /*for sending message*/
    MsgHeader(uint16_t id,
              const Container &input,
              MsgNodeType type = MsgNodeType::MSGNODE_NORMAL) noexcept
        : _id(id)
        , _type(type)
        , _cur_length(0)
    {
        _length = input.size() + get_header_length();
        _buffer.resize(_length, 0);
    }

    static std::size_t MSGNODE_NORMAL_HEADER_LENGTH;
    static std::size_t MSGNODE_FILE_HEADER_LENGTH;

    std::size_t get_header_length() {
        if(this->_type == MsgNodeType::MSGNODE_FILE_TRANSFER){
            return MSGNODE_FILE_HEADER_LENGTH;
        }
        return MSGNODE_NORMAL_HEADER_LENGTH;
    }

    typename std::iterator_traits<typename Container::iterator>::pointer
    get_header_base() {
        return _buffer.data();
    }
    typename std::iterator_traits<typename Container::iterator>::pointer
    get_body_base() {
        return get_header_base() + this->get_header_length();
    }

    /*please be aware, it should be network sequence, please convert network
   * sequence to host*/
    virtual std::optional<uint16_t> get_id() {
        if (check_header_remaining()) { /*not OK*/
            return std::nullopt;
        }
        _id = *(reinterpret_cast<uint16_t *>(get_id_base()));
        return _id;
    }

    /*
   * when user deploy gen_length, it will ONLY return the size of message!
   * please be aware, it should be network sequence, please convert network
   * sequence to host
   */
    virtual std::optional<uint16_t> get_length() {
        if (check_header_remaining()) { /*not OK*/
            return std::nullopt;
        }

        uint16_t total_length = *(reinterpret_cast<uint16_t *>(get_length_base()));

        /*we only have the header length*/
        if (_length < total_length) {
            _length = total_length;

            /*extend the size of buffer*/
            _buffer.resize(_length, 0);
        }
        return _length - this->get_header_length();
    }

    virtual std::optional<Container> get_msg_body() {
        if (check_body_remaining()) {
            return std::nullopt;
        }

        /*unfortunely, QByteArray does not support assign
     * QString is an exception
     * auto ib = _buffer.begin();
     * auto ie = _buffer.begin();
     * std::advance(ib, get_header_length());
     * std::advance(ie, _length);
     * Container ret;
     * ret.assign(ib, ie);
     */
        if constexpr (std::is_same_v<Container, QString>) {
            return this->_buffer.mid(this->get_header_length(), _length);
        }
        return Container(get_body_base(), _length - this->get_header_length());
    }

    void update_pointer_pos(const uint16_t increment) {
        _cur_length += increment;
    }

    void clear() {
        /*clean previous packet*/
        auto ie = _buffer.begin();
        std::advance(ie, _length);
        _buffer.erase(_buffer.begin(), ie);

        /*clear all the data*/
        _cur_length = 0;
        _length = 0;
    }

    const bool check_header_remaining() {
        return !(_cur_length >= this->get_header_length());
    }

    const bool check_body_remaining(){
        return !(_cur_length >= _length);
    }

protected:
    typename std::iterator_traits<typename Container::iterator>::pointer
    get_id_base() {
        return get_header_base();
    }

    typename std::iterator_traits<typename Container::iterator>::pointer
    get_length_base() {
        return get_header_base() + sizeof(uint16_t);
    }

public:
    uint16_t _id;

protected:
    /* ---------------------------------------------
   * name |  _id  |  _length  |     _buffer      |
   * size |   2B  |   2B(4B)  | _length - 4B(6B) |
   * --------------------------------------------*/
    uint16_t _length; /*total length*/
    uint16_t _cur_length;
    Container _buffer;
    MsgNodeType _type = MsgNodeType::MSGNODE_NORMAL;
};

template <typename Container, typename Callable>
class RecvNode<
    Container, Callable,
    typename std::enable_if<recv_msg_check<Container>::value, void>::type>
    : public MsgHeader<Container> {

public:
    RecvNode(Callable &&Network2Host, MsgNodeType type = MsgNodeType::MSGNODE_NORMAL) noexcept
        : m_convertor(std::move(Network2Host)), MsgHeader<Container>(type) {}

    virtual std::optional<uint16_t> get_id() {
        if (this->check_header_remaining()) { /*not OK*/
            return std::nullopt;
        }

        /*update converted id*/
        this->_id = *(reinterpret_cast<uint16_t *>(this->get_id_base()));
        this->_id = m_convertor(this->_id);
        return this->_id;
    }

    /*
   * when user deploy gen_length, it will ONLY return the size of message!
   */
    virtual std::optional<uint16_t> get_length() {
        if (this->check_header_remaining()) { /*not OK*/
            return std::nullopt;
        }

        /*update converted full length*/
        this->_length = *(reinterpret_cast<uint16_t *>(this->get_length_base()));
        this->_length = m_convertor(this->_length);

        /*don't forgot to resize*/
        this->_buffer.resize(this->_length, 0);

        /*we only need the header length*/
        return this->_length - this->get_header_length();
    }

    virtual std::optional<Container> get_msg_body() {
        if (this->check_body_remaining()) {
            return std::nullopt;
        }

        // Container temporary;
        if constexpr (std::is_same_v<Container, QString>)
            return this->_buffer.mid(this->get_header_length(), this->_length);
        else
            return Container(this->get_body_base(),
                             this->_length - this->get_header_length());
    }

private:
    Callable m_convertor;
};

template <typename Container, typename Callable>
struct SendNode<
    Container, Callable,
    typename std::enable_if<send_msg_check<Container>::value, void>::type>
    : public MsgHeader<Container> {

    SendNode(uint16_t msg_id,
             Container &string,
             Callable &&Host2Network,
             MsgNodeType type = MsgNodeType::MSGNODE_NORMAL) noexcept
        : m_convertor(std::move(Host2Network)),
        MsgHeader<Container>(msg_id, string, type) {

        /*we should not change the inner variables*/
        uint16_t cv_id = m_convertor(this->_id);
        uint16_t cv_length = m_convertor(this->_length);

        *reinterpret_cast<uint16_t *>(this->get_header_base()) = cv_id;
        *reinterpret_cast<uint16_t *>(this->get_length_base()) = cv_length;

        /*msg body base*/
        auto it = this->_buffer.begin();
        std::advance(it, this->get_header_length());

        std::copy(string.begin(), string.end(), it);
    }
    const Container &get_buffer() const { return this->_buffer; }
    const std::size_t get_full_length() const { return this->_length; }

private:
    Callable m_convertor;
};

template <typename Container>
std::size_t MsgHeader<Container>::MSGNODE_NORMAL_HEADER_LENGTH =
    sizeof(uint16_t) + sizeof(uint16_t);

template <typename Container>
std::size_t MsgHeader<Container>::MSGNODE_FILE_HEADER_LENGTH =
    sizeof(uint16_t) + sizeof(uint32_t);

#endif
