#pragma once
#ifndef _MYSQLCONNECTION_HPP_
#define _MYSQLCONNECTION_HPP_
#include <boost/asio/io_context.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/mysql/tcp_ssl.hpp>
#include <chrono>
#include <map>
#include <optional>
#include <server/UserNameCard.hpp>
#include <string_view>
#include <vector>

/*delcaration*/
struct UserNameCard;
struct UserFriendRequest;

struct MySQLRequestStruct {
  std::string_view m_username;
  std::string_view m_password;
  std::string_view m_email;
};

namespace mysql {
class MySQLConnectionPool;

enum class MySQLSelection : uint8_t {
  HEART_BEAT,
  FIND_EXISTING_USER,
  CREATE_NEW_USER,    // register new user
  UPDATE_USER_PASSWD, // update user password
  USER_LOGIN_CHECK,   // check login username & password
  USER_UUID_CHECK,    // check account uuid in DB
  USER_PROFILE,       // check account user profile
  GET_USER_UUID,      // get uuid by username

  CREATE_FRIENDING_REQUEST,     // User A send friend request to B, status = 0
  UPDATE_FRIEND_REQUEST_STATUS, // User A agreed with B's request, then change
                                // status = 1
  CREATE_AUTH_FRIEND_ENTRY, // After update status, add user A & B's info to
                            // AuthFriend Table

  GET_FRIEND_REQUEST_LIST, // Get User's Request List
  GET_AUTH_FRIEND_LIST,    // Get User's Auth Friend List
};

class MySQLConnection {
  friend class MySQLConnectionPool;
  MySQLConnection(const MySQLConnection &) = delete;
  MySQLConnection &operator=(const MySQLConnection &) = delete;

public:
  MySQLConnection(std::string_view username, std::string_view password,
                  std::string_view database, std::string_view host,
                  std::string_view port,
                  mysql::MySQLConnectionPool *shared) noexcept;

  ~MySQLConnection();

public:
  /*get user profile*/
  std::optional<std::unique_ptr<UserNameCard>> getUserProfile(std::size_t uuid);

  /*create user friend request MySQLSelection::USER_FRIEND_REQUEST*/
  bool createFriendRequest(const std::size_t src_uuid,
                           const std::size_t dst_uuid,
                           std::string_view nickname, std::string_view message);

  /*update user friend request to confirmed status*/
  bool updateFriendingStatus(const std::size_t src_uuid,
                             const std::size_t dst_uuid);

  bool createAuthFriendsRelation(const std::size_t self_uuid,
                                 const std::size_t friend_uuid,
                                 const std::string &alternative);

  /*
   * get specfic amount of friending request
   * which is restricted by a startpos and an interval
   * the valid data is from [starpos, startpos + interval - 1]
   */
  std::optional<std::vector<std::unique_ptr<UserFriendRequest>>>
  getFriendingRequestList(const std::size_t dst_uuid,
                          const std::size_t start_pos,
                          const std::size_t interval);

  std::optional<std::vector<std::unique_ptr<UserNameCard>>>
  getAuthenticFriendsList(const std::size_t self_uuid,
                          const std::size_t start_pos,
                          const std::size_t interval);

  /*insert new user, call MySQLSelection::CREATE_NEW_USER*/
  bool registerNewUser(MySQLRequestStruct &&request);
  bool alterUserPassword(MySQLRequestStruct &&request);

  /*login username & password check*/
  std::optional<std::size_t> checkAccountLogin(std::string_view username,
                                               std::string_view password);

  /*is username and email were occupied!*/
  bool checkAccountAvailability(std::string_view username,
                                std::string_view email);

  bool checkTimeout(const std::chrono::steady_clock::time_point &curr,
                    std::size_t timeout);

  bool checkUUID(std::size_t uuid);

  std::optional<std::size_t> getUUIDByUsername(std::string_view username);
  std::optional<std::string> getUsernameByUUID(std::size_t uuid);

private:
  template <typename... Args>
  std::optional<boost::mysql::results> executeCommand(MySQLSelection select,
                                                      Args &&...args);

  void updateTimer();

  /*send heart packet to mysql to prevent from disconnecting*/
  bool sendHeartBeat();

private:
  std::shared_ptr<MySQLConnectionPool> m_delegator;

  // The execution context, required to run I/O operations.
  boost::asio::io_context &ctx;

  // The SSL context, required to establish TLS connections.
  // The default SSL options are good enough for us at this point.
  boost::asio::ssl::context ssl_ctx;

  // Represents a connection to the MySQL server.
  boost::mysql::tcp_ssl_connection conn;

  /*last operation time*/
  std::chrono::steady_clock::time_point last_operation_time;
};
} // namespace mysql

#endif // !_MYSQLCONNECTION_HPP_
