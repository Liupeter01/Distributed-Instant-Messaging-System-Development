#pragma once
#ifndef _REQUEST_HANDLER_DISPATCHER_HPP_
#define _REQUEST_HANDLER_DISPATCHER_HPP_
#include <handler/RequestHandlerNode.hpp>
#include <singleton/singleton.hpp>

namespace dispatcher {

class RequestHandlerDispatcher : public Singleton<RequestHandlerDispatcher> {

  friend class Singleton<RequestHandlerDispatcher>;
  RequestHandlerDispatcher();

public:
  virtual ~RequestHandlerDispatcher();

public:
private:
};
} // namespace dispatcher

#endif
