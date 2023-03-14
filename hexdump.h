#ifndef HEXDUMP_H
#define HEXDUMP_H

namespace neolib
{
  template<class Elem, class Traits>
  inline void hex_dump(const void* aData, std::size_t aLength, std::basic_ostream<Elem, Traits>& aStream, std::size_t aWidth = 16)
  {
    const char* const start = static_cast<const char*>(aData);
    const char* const end = start + aLength;
    const char* line = start;
    while (line != end)
    {
      aStream.width(4);
      aStream.fill('0');
      aStream << std::hex << line - start << " : ";
      std::size_t lineLength = std::min(aWidth, static_cast<std::size_t>(end - line));
      for (const char* next = line; next != end && next != line + aWidth; ++next)
      {
        char ch = *next;
        if (next != line)
          aStream << " ";
        aStream.width(2);
        aStream.fill('0');
        aStream << std::hex << std::uppercase << static_cast<int>(static_cast<unsigned char>(ch));
      }
      if (lineLength != aWidth)
        aStream << std::string(aWidth - lineLength, ' ');
      aStream << " ";

      aStream << std::endl;
      line = line + lineLength;
    }
  }
}
#endif // HEXDUMP_H
