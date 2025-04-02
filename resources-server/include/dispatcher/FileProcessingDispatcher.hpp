#pragma once
#ifndef _FILE_PROCESSING_DISPATCHER_HPP_
#define _FILE_PROCESSING_DISPATCHER_HPP_
#include <handler/FileProcessingNode.hpp>
#include <singleton/singleton.hpp>

namespace dispatcher {

class FileProcessingDispatcher : public Singleton<FileProcessingDispatcher> {
  friend class Singleton<FileProcessingDispatcher>;

  FileProcessingDispatcher();

public:
  virtual ~FileProcessingDispatcher();

private:
};

} // namespace dispatcher

#endif
