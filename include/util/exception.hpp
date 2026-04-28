
#ifndef LOST_EXCEPTION_HPP_
#define LOST_EXCEPTION_HPP_

#include <string>

namespace lost {

class Exception : public std::exception {
private:
	std::string _sMessage;
public:
	Exception();
	Exception(const std::string& sErrorMessage);
	const char* what() const throw();
	void setMessage(const std::string& sMessage);
	virtual ~Exception() throw();
};

} /* namespace lost */

#endif /* LOST_EXCEPTION_HPP_ */
