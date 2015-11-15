//
// Created by Vladimir on 8/2/2015.
//

#ifndef SOCKET_SOCKET_H
#define SOCKET_SOCKET_H

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <memory>
#include <unistd.h>
#include <vector>
#include <functional>

namespace IO
{
class Socket
{
	public:
	struct AcceptError {
		int fd;
		const Socket *ptr;
	};
	struct WriteError {
		int fd;
		const Socket *ptr;
	};
	struct ConnectionClosedByPeer {
		int fd;
		const Socket *ptr;
	};

	Socket(int);
	Socket(int port, int);
	Socket(const Socket &) = delete;
	Socket(Socket &&other) : fd_(-1) { *this = std::move(other); }
	Socket &operator=(Socket &&other)
	{
		if (this != &other) {
			Close();
			fd_ = other.fd_;
			port_ = other.port_;
			address_ = other.address_;
			connection_ = other.connection_;
			other.fd_ = -1;
		}
		return *this;
	}
	Socket &operator=(const Socket &) = delete;
	bool operator<(const Socket &) const;
	bool operator==(const Socket &) const;
	operator bool() const;
	virtual ~Socket();
	Socket Accept() const;
	Socket Duplicate() const noexcept;
	int GetFD() const;
	bool IsAcceptor() const;
	void Bind() const;
	void MakeNonBlocking() const;
	void Listen(int pending_max) const;
	long AvailableToRead() const;
	bool WasShutDown() const;
	void Close();

	static Socket start_socket(int port, int maxConnections);

	template <class T> T ReadSome() const
	{
		T result;
		ssize_t available = 0;
		try {
			available = static_cast<std::size_t>(AvailableToRead());
		} catch (std::runtime_error &ex) {
			throw;
		}
		result.resize(available);
		ssize_t readBytes = ::read(fd_, &result.front(), available);
		if (available != readBytes)
			throw std::runtime_error("Socket read error on fd = " + std::to_string(fd_) +
						 "."
						 "Expected to read " +
						 std::to_string(available) + " bytes, but could only read " +
						 std::to_string(readBytes) + " bytes");
		return result;
	}

	template <class T> T Read(std::size_t size = 0) const
	{
		T result;
		result.resize(size);
		auto readBytes = ::read(fd_, &result.front(), size);
		if (readBytes == 0)
			throw ConnectionClosedByPeer{fd_, this};
		if (readBytes == -1) {
			if (!(errno == EAGAIN || errno == EWOULDBLOCK))
				throw std::runtime_error("Error when reading from socket, "
							 "errno = " +
							 std::to_string(errno));
		} else if (static_cast<std::size_t>(readBytes) != size)
			result.resize(readBytes);
		return result;
	}

	std::string ReadUntil(const std::string &until, bool peek = false) const
	{
		std::string result;
		constexpr std::size_t buffSize = 20;
		std::size_t sum = 0;
		do {
			result.resize(sum + buffSize);
			auto bytesRead = ::recv(fd_, &result.front(), buffSize + sum, MSG_PEEK);
			if (bytesRead == 0)
				throw ConnectionClosedByPeer{fd_, this};
			if (bytesRead == -1) {
				if (!(errno == EAGAIN || errno == EWOULDBLOCK))
					throw std::runtime_error("Error when reading from socket, "
								 "errno = " +
								 std::to_string(errno));
				else
					return "";
			}
			auto position = result.find(until.c_str(), 0, until.size());
			if (position != std::string::npos) {
				position += until.size();
				try {
					if (peek)
						return result.substr(0, position);
					return Read<std::string>(position);
				} catch (std::runtime_error &ex) {
					throw;
				}
			} else
				sum += bytesRead;
		} while (true);
	}

	template <typename T> std::size_t Write(const T &data) const
	{
		auto written = ::send(fd_, static_cast<const void *>(data.data()), data.size(), MSG_NOSIGNAL);
		if (written == -1) {
			if (!(errno == EAGAIN || errno == EWOULDBLOCK))
				throw WriteError{fd_, this};
			if (errno == ECONNRESET)
				throw ConnectionClosedByPeer{fd_, this};
			if (errno == EPIPE)
				throw ConnectionClosedByPeer{fd_, this};
		}
		return written == -1 ? 0 : written;
	}

	private:
	int fd_ = -1;
	int port_;
	bool connection_ = false;
	struct sockaddr_in address_;
};
typedef std::reference_wrapper<Socket> SocketRef;
}

#endif // SOCKET_SOCKET_H
