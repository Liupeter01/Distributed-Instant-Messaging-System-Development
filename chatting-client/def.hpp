#pragma once
#ifndef _DEF_HPP_
#include <stdint.h>

enum class ServiceType : uint8_t {
  SERVICE_VERIFICATION,  // get verification code
  SERVICE_REGISTERATION, // user registeration
  SERVICE_CHECKEEXISTS,  // check account existance
  SERVICE_RESETPASSWD,   // password reset

  /*
   * before login, user has to get the address of
   * chatting server which is dispatched by balance-server
   */
  SERVICE_LOGINDISPATCH,
  SERVICE_LOGINSERVER,            // try to login into server
  SERVICE_LOGINRESPONSE,          // login server response
  SERVICE_LOGOUTSERVER,           // try to logout into server
  SERVICE_LOGOUTRESPONSE,         // logout server response
  SERVICE_SEARCHUSERNAME,         // client search another user's username
  SERVICE_SEARCHUSERNAMERESPONSE, //

  /*
   * User A init a friending request to User B
   * However this message will only be received by Server Directly
   * If the forwarding successful, then send SERVICE_FRIENDSENDERRESPONSE to
   * User A
   */
  SERVICE_FRIENDREQUESTSENDER,
  SERVICE_FRIENDSENDERRESPONSE,

  /*The Server Produce this request only!
   * The Server will Forward A's Friending Request to B's interface
   */
  SERVICE_FRIENDREINCOMINGREQUEST,

  /*
   * User B confirm A's friending request
   * However this message will only be received by Server Directly
   */
  SERVICE_FRIENDREQUESTCONFIRM,

  /*If the forwarding successful, then send SERVICE_FRIENDSENDERRESPONSE to User
         B*/
  SERVICE_FRIENDCONFIRMRESPONSE,

  /*if the friending process success, then both src_uuid and dst_uuid could
         receive this*/
  SERVICE_FRIENDING_ON_BIDDIRECTIONAL,

  /*User send text chat msg request*/
  SERVICE_TEXTCHATMSGREQUEST,
  SERVICE_TEXTCHATMSGRESPONSE,

  /*The Server Produce this request only!
   * The Server will Forward A's Text Msg to B
   */
  SERVICE_TEXTCHATMSGICOMINGREQUEST,

  /*User send text chat msg request*/
  SERVICE_VOICECHATMSGREQUEST,
  SERVICE_VOICETCHATMSGRESPONSE,

  /*User send text chat msg request*/
  SERVICE_VIDEOCHATMSGREQUEST,
  SERVICE_VIDEOCHATMSGRESPONSE,

  /*User send file upload request*/
  SERVICE_FILEUPLOADREQUEST,
  SERVICE_FILEUPLOADRESPONSE,

  SERVICE_UNKNOWN // unkown service
};

enum class ServiceStatus : uint8_t {
  SERVICE_SUCCESS,                // SUCCESS
  JSONPARSE_ERROR,                // json parsing error
  NETWORK_ERROR,                  // network failed
  GRPC_ERROR,                     // grpc error
  GRPC_SERVER_NOT_EXISTS,         // GRPC server not exist
  GRPC_SERVER_ALREADY_EXISTS,     // GRPC server already exists
  CHATTING_SERVER_NOT_EXISTS,     // chatting server not exist
  CHATTING_SERVER_ALREADY_EXISTS, // chatting server already exists
      NO_AVAILABLE_CHATTING_SERVER,         //chatting server no available
  REDIS_UNKOWN_ERROR,             // redis server error code
  REDIS_CPATCHA_NOT_FOUND,        // redis no cpatcha
  MYSQL_INTERNAL_ERROR,           // mysql error
  MYSQL_MISSING_INFO,             // mysql missing account info
  MYSQL_ACCOUNT_NOT_EXISTS,       // mysql account not exists
  LOGIN_INFO_ERROR,               // login info error
  LOGIN_UNSUCCESSFUL, // common login uncessfully reason, due to internel error
  LOGIN_FOR_MULTIPLE_TIMES, // this user have already logined!
  LOGOUT_UNSUCCESSFUL,      // common logout uncessfully reason, due to internel
  // error
  SEARCHING_USERNAME_NOT_FOUND, // client search another user's username not
  // found
  FRIENDING_YOURSELF,              // user should not friending itself
  FRIENDING_ERROR,                 // friending error
  FRIENDING_TARGET_USER_NOT_FOUND, // target user not found

  FILE_UPLOAD_ERROR, // file upload error
  FILE_CREATE_ERROR,
  FILE_OPEN_ERROR,
  FILE_WRITE_ERROR
};

#define _DEF_HPP_
#endif
