#define EZC3D_API_EXPORTS
#include "ezc3d.h"

ezc3d::c3d::c3d():
    _filePath("")
{
    _header = std::shared_ptr<ezc3d::Header>(new ezc3d::Header());
    _parameters = std::shared_ptr<ezc3d::ParametersNS::Parameters>(new ezc3d::ParametersNS::Parameters());
    _data = std::shared_ptr<ezc3d::DataNS::Data>(new ezc3d::DataNS::Data());
}

ezc3d::c3d::c3d(const std::string &filePath):
    std::fstream(filePath, std::ios::in | std::ios::binary),
    _filePath(filePath)
{
    if (!is_open())
        throw std::ios_base::failure("Could not open the c3d file");

    // Read all the section
    _header = std::shared_ptr<ezc3d::Header>(new ezc3d::Header(*this));
    _parameters = std::shared_ptr<ezc3d::ParametersNS::Parameters>(new ezc3d::ParametersNS::Parameters(*this));
    _data = std::shared_ptr<ezc3d::DataNS::Data>(new ezc3d::DataNS::Data(*this));
}


ezc3d::c3d::~c3d()
{
    close();
}

void ezc3d::c3d::write(const std::string& filePath) const
{
    std::fstream f(filePath, std::ios::out | std::ios::binary);

    // Write the header
    this->header().write(f);

    // Write the parameters
    this->parameters().write(f);

    // Write the data
    this->data().write(f);

    f.close();
}

void ezc3d::removeSpacesOfAString(std::string& s){
    // Remove the spaces at the end of the strings
    for (size_t i = s.size(); i >= 0; --i)
        if (s[s.size()-1] == ' ')
            s.pop_back();
        else
            break;
}
std::string ezc3d::toUpper(const std::string &str){
    std::string new_str = str;
    std::transform(new_str.begin(), new_str.end(), new_str.begin(), ::toupper);
    return new_str;
}


unsigned int ezc3d::c3d::hex2uint(const char * val, int len){
    int ret(0);
    for (int i=0; i<len; i++)
        ret |= int((unsigned char)val[i]) * int(pow(0x100, i));
    return ret;
}

int ezc3d::c3d::hex2int(const char * val, int len){
    unsigned int tp(hex2uint(val, len));

    // convert to signed int
    // Find max int value
    unsigned int max(0);
    for (int i=0; i<len; ++i)
        max |= 0xFF * int(pow(0x100, i));

    // If the value is over uint_max / 2 then it is a negative number
    int out;
    if (tp > max / 2)
        out = (int)(tp - max - 1);
    else
        out = tp;

    return out;
}

int ezc3d::c3d::hex2long(const char * val, int len){
    long ret(0);
    for (int i=0; i<len; i++)
        ret |= long((unsigned char)val[i]) * long(pow(0x100, i));
    return ret;
}

void ezc3d::c3d::readFile(int nByteToRead, char * c, int nByteFromPrevious,
                     const  std::ios_base::seekdir &pos)
{
    if (pos != 1)
        this->seekg (nByteFromPrevious, pos); // Move to number analogs
    this->read (c, nByteToRead);
    c[nByteToRead] = '\0'; // Make sure last char is NULL
}
void ezc3d::c3d::readChar(int nByteToRead, char * c,int nByteFromPrevious,
                     const  std::ios_base::seekdir &pos)
{
    c = new char[nByteToRead + 1];
    readFile(nByteToRead, c, nByteFromPrevious, pos);
}

std::string ezc3d::c3d::readString(int nByteToRead, int nByteFromPrevious,
                              const std::ios_base::seekdir &pos)
{
    char* c = new char[nByteToRead + 1];
    readFile(nByteToRead, c, nByteFromPrevious, pos);
    std::string out(c);
    delete c;
    return out;
}

int ezc3d::c3d::readInt(int nByteToRead, int nByteFromPrevious,
            const std::ios_base::seekdir &pos)
{
    char* c = new char[nByteToRead + 1];
    readFile(nByteToRead, c, nByteFromPrevious, pos);

    // make sure it is an int and not an unsigned int
    int out(hex2int(c, nByteToRead));
    delete c;
    return out;
}

int ezc3d::c3d::readUint(int nByteToRead, int nByteFromPrevious,
            const std::ios_base::seekdir &pos)
{
    char* c = new char[nByteToRead + 1];
    readFile(nByteToRead, c, nByteFromPrevious, pos);

    // make sure it is an int and not an unsigned int
    int out(hex2uint(c, nByteToRead));
    delete c;
    return out;
}

float ezc3d::c3d::readFloat(int nByteFromPrevious,
                const std::ios_base::seekdir &pos)
{
    int nByteToRead(4*ezc3d::DATA_TYPE::BYTE);
    char* c = new char[nByteToRead + 1];
    readFile(nByteToRead, c, nByteFromPrevious, pos);
    float out (*reinterpret_cast<float*>(c));
    delete c;
    return out;
}

void ezc3d::c3d::readMatrix(int dataLenghtInBytes, const std::vector<int> &dimension,
                       std::vector<int> &param_data, int currentIdx)
{
    for (int i=0; i<dimension[currentIdx]; ++i)
        if (currentIdx == dimension.size()-1)
            param_data.push_back (readInt(dataLenghtInBytes*ezc3d::DATA_TYPE::BYTE));
        else
            readMatrix(dataLenghtInBytes, dimension, param_data, currentIdx + 1);
}

void ezc3d::c3d::readMatrix(const std::vector<int> &dimension,
                       std::vector<float> &param_data, int currentIdx)
{
    for (int i=0; i<dimension[currentIdx]; ++i)
        if (currentIdx == dimension.size()-1)
            param_data.push_back (readFloat());
        else
            readMatrix(dimension, param_data, currentIdx + 1);
}

void ezc3d::c3d::readMatrix(const std::vector<int> &dimension,
                       std::vector<std::string> &param_data_string)
{
    std::vector<std::string> param_data_string_tp;
    _readMatrix(dimension, param_data_string_tp);

    // Vicon c3d stores text length on first dimension, I am not sure if
    // this is a standard or a custom made stuff. I implemented it like that for now
    if (dimension.size() == 1){
        std::string tp;
        for (int j = 0; j < dimension[0]; ++j)
            tp += param_data_string_tp[j];
        ezc3d::removeSpacesOfAString(tp);
        param_data_string.push_back(tp);
    }
    else
        _dispatchMatrix(dimension, param_data_string_tp, param_data_string);
}

void ezc3d::c3d::_readMatrix(const std::vector<int> &dimension,
                       std::vector<std::string> &param_data, int currentIdx)
{
    for (int i=0; i<dimension[currentIdx]; ++i)
        if (currentIdx == dimension.size()-1)
            param_data.push_back(readString(ezc3d::DATA_TYPE::BYTE));
        else
            _readMatrix(dimension, param_data, currentIdx + 1);
}

int ezc3d::c3d::_dispatchMatrix(const std::vector<int> &dimension,
                                 const std::vector<std::string> &param_data_in,
                                 std::vector<std::string> &param_data_out, int idxInParam,
                                 int currentIdx)
{
    for (int i=0; i<dimension[currentIdx]; ++i)
        if (currentIdx == dimension.size()-1){
            std::string tp;
            for (int j = 0; j < dimension[0]; ++j){
                tp += param_data_in[idxInParam];
                ++idxInParam;
            }
            ezc3d::removeSpacesOfAString(tp);
            param_data_out.push_back(tp);
        }
        else
            idxInParam = _dispatchMatrix(dimension, param_data_in, param_data_out, idxInParam, currentIdx + 1);
    return idxInParam;
}

const ezc3d::Header& ezc3d::c3d::header() const
{
    return *_header;
}

const ezc3d::ParametersNS::Parameters& ezc3d::c3d::parameters() const
{
    return *_parameters;
}

const ezc3d::DataNS::Data& ezc3d::c3d::data() const
{
    return *_data;
}

