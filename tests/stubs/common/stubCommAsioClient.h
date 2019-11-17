// AUTOGENERATED file, created by the tool generate_stub.py, don't edit!
// If you want to add your own functionality, instead edit the stubCommAsioClient_custom.h file.

#ifndef STUB_COMMON_COMMASIOCLIENT_H
#define STUB_COMMON_COMMASIOCLIENT_H

#include "common/CommAsioClient.h"
#include "stubCommAsioClient_custom.h"

#ifndef STUB_CommAsioClient_CommAsioClient
//#define STUB_CommAsioClient_CommAsioClient
  template <typename ProtocolT>
   CommAsioClient<ProtocolT>::CommAsioClient(std::string name, boost::asio::io_context& io_context, const Atlas::Objects::Factories& factories)
    : Atlas::Objects::ObjectsDecoder(name, io_context, factories)
  {
    
  }
#endif //STUB_CommAsioClient_CommAsioClient

#ifndef STUB_CommAsioClient_CommAsioClient_DTOR
//#define STUB_CommAsioClient_CommAsioClient_DTOR
  template <typename ProtocolT>
   CommAsioClient<ProtocolT>::~CommAsioClient()
  {
    
  }
#endif //STUB_CommAsioClient_CommAsioClient_DTOR

#ifndef STUB_CommAsioClient_getSocket
//#define STUB_CommAsioClient_getSocket
  template <typename ProtocolT>
  ProtocolT::socket& CommAsioClient<ProtocolT>::getSocket()
  {
    return *static_cast<ProtocolT::socket*>(nullptr);
  }
#endif //STUB_CommAsioClient_getSocket

#ifndef STUB_CommAsioClient_startAccept
//#define STUB_CommAsioClient_startAccept
  template <typename ProtocolT>
  void CommAsioClient<ProtocolT>::startAccept(std::unique_ptr<Link> connection)
  {
    
  }
#endif //STUB_CommAsioClient_startAccept

#ifndef STUB_CommAsioClient_startConnect
//#define STUB_CommAsioClient_startConnect
  template <typename ProtocolT>
  void CommAsioClient<ProtocolT>::startConnect(std::unique_ptr<Link> connection)
  {
    
  }
#endif //STUB_CommAsioClient_startConnect

#ifndef STUB_CommAsioClient_send
//#define STUB_CommAsioClient_send
  template <typename ProtocolT>
  int CommAsioClient<ProtocolT>::send(const Atlas::Objects::Operation::RootOperation&)
  {
    return 0;
  }
#endif //STUB_CommAsioClient_send

#ifndef STUB_CommAsioClient_disconnect
//#define STUB_CommAsioClient_disconnect
  template <typename ProtocolT>
  void CommAsioClient<ProtocolT>::disconnect()
  {
    
  }
#endif //STUB_CommAsioClient_disconnect

#ifndef STUB_CommAsioClient_flush
//#define STUB_CommAsioClient_flush
  template <typename ProtocolT>
  int CommAsioClient<ProtocolT>::flush()
  {
    return 0;
  }
#endif //STUB_CommAsioClient_flush

#ifndef STUB_CommAsioClient_do_read
//#define STUB_CommAsioClient_do_read
  template <typename ProtocolT>
  void CommAsioClient<ProtocolT>::do_read()
  {
    
  }
#endif //STUB_CommAsioClient_do_read

#ifndef STUB_CommAsioClient_write
//#define STUB_CommAsioClient_write
  template <typename ProtocolT>
  void CommAsioClient<ProtocolT>::write()
  {
    
  }
#endif //STUB_CommAsioClient_write

#ifndef STUB_CommAsioClient_dispatch
//#define STUB_CommAsioClient_dispatch
  template <typename ProtocolT>
  void CommAsioClient<ProtocolT>::dispatch()
  {
    
  }
#endif //STUB_CommAsioClient_dispatch

#ifndef STUB_CommAsioClient_startNegotiation
//#define STUB_CommAsioClient_startNegotiation
  template <typename ProtocolT>
  void CommAsioClient<ProtocolT>::startNegotiation()
  {
    
  }
#endif //STUB_CommAsioClient_startNegotiation

#ifndef STUB_CommAsioClient_negotiate
//#define STUB_CommAsioClient_negotiate
  template <typename ProtocolT>
  int CommAsioClient<ProtocolT>::negotiate()
  {
    return 0;
  }
#endif //STUB_CommAsioClient_negotiate

#ifndef STUB_CommAsioClient_negotiate_read
//#define STUB_CommAsioClient_negotiate_read
  template <typename ProtocolT>
  void CommAsioClient<ProtocolT>::negotiate_read()
  {
    
  }
#endif //STUB_CommAsioClient_negotiate_read

#ifndef STUB_CommAsioClient_negotiate_write
//#define STUB_CommAsioClient_negotiate_write
  template <typename ProtocolT>
  void CommAsioClient<ProtocolT>::negotiate_write()
  {
    
  }
#endif //STUB_CommAsioClient_negotiate_write

#ifndef STUB_CommAsioClient_operation
//#define STUB_CommAsioClient_operation
  template <typename ProtocolT>
  int CommAsioClient<ProtocolT>::operation(const Atlas::Objects::Operation::RootOperation&)
  {
    return 0;
  }
#endif //STUB_CommAsioClient_operation

#ifndef STUB_CommAsioClient_objectArrived
//#define STUB_CommAsioClient_objectArrived
  template <typename ProtocolT>
  void CommAsioClient<ProtocolT>::objectArrived(const Atlas::Objects::Root& obj)
  {
    
  }
#endif //STUB_CommAsioClient_objectArrived


#endif