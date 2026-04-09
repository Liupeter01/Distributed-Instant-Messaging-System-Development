#include <server/FileHasherLogger.hpp>

namespace handler {

          FileHasherDesc::FileHasherDesc(const std::string& _filename, const std::string& _checksum, const std::string& _filePath,
                    const std::string& _curr_sequence,
                    const std::string& _last_sequence, const std::string& _eof,
                    std::size_t _transfered_size, std::size_t  _current_block_size, std::size_t _total_size)
                    : filename(_filename), checksum(_checksum), filePath(_filePath),curr_sequence(_curr_sequence),
                    last_sequence(_last_sequence), transfered_size(_transfered_size),
                    current_block_size(_current_block_size),
                    total_size(_total_size), isEOF(_eof), key(_filename + std::string("_") + _checksum)
          {
          }

          FileDescriptionBlock::FileDescriptionBlock(const FileHasherDesc& o,
                    const std::string& block,
                    std::function<void(const ServiceStatus)>&& _callback)
                    : FileHasherDesc(o), block_data(block), callback(std::move(_callback))
          {
          }

}