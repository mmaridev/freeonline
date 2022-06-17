/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#include "Protocol.hpp"
#include "config.h"

#include "HttpRequest.hpp"
#include "helpers.hpp"
#include "Log.hpp"
#include "Unit.hpp"
#include "UnitHTTP.hpp"
#include "Util.hpp"

#include <Poco/DateTimeFormat.h>
#include <Poco/DateTimeFormatter.h>
#include <Poco/JSON/Object.h>
#include <Poco/MemoryStream.h>
#include <Poco/RegularExpression.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/URI.h>
#include <Poco/Util/LayeredConfiguration.h>
#include <sstream>
#include <vector>

/// Simulates a WOPI server for testing purposes.
/// Currently only serves one file contents.
/// Furthermore, the file URI doesn't contain the
/// real filename (in most tests), instead filenames
/// 1 to 9 are considered special.
class WopiTestServer : public UnitWSD
{
private:

    enum class LOOLStatusCode
    {
        DocChanged = 1010
    };

    /// The WOPISrc URL.
    std::string _wopiSrc;

    /// Websockets to communicate.
    std::vector< std::unique_ptr<UnitWebSocket> > _wsList;

    /// Content of the file.
    std::string _fileContent;

    /// The filename. TODO: Support multiple ones.
    std::string _filename;

    /// Last modified time of the file
    std::chrono::system_clock::time_point _fileLastModifiedTime;

    /// The number of CheckFileInfo invocations.
    std::size_t _countCheckFileInfo;
    /// The number of GetFile invocations.
    std::size_t _countGetFile;
    /// The number of rename invocations.
    std::size_t _countPutRelative;
    /// The number of upload invocations.
    std::size_t _countPutFile;

    /// The default filename when only content is given.
    static constexpr auto DefaultFilename = "hello.txt";

protected:

    const std::string& getWopiSrc() const { return _wopiSrc; }

    const std::unique_ptr<UnitWebSocket>& getWs() const { return _wsList.at(0); }

    const std::unique_ptr<UnitWebSocket>& getWsAt(int index) { return _wsList.at(index); }

    void deleteSocketAt(int index)
    {
        std::unique_ptr<UnitWebSocket>& socket = _wsList.at(index);
        socket.reset();
    }

    const std::string& getFileContent() const { return _fileContent; }

    /// Sets the file content to a given value and update the last file modified time
    void setFileContent(const std::string& fileContent)
    {
        LOG_TST("setFileContent: [" << LOOLProtocol::getAbbreviatedMessage(fileContent ) << ']');
        _fileContent = fileContent;
        _fileLastModifiedTime = std::chrono::system_clock::now();
    }

    const std::chrono::system_clock::time_point& getFileLastModifiedTime() const
    {
        return _fileLastModifiedTime;
    }

    WopiTestServer(const std::string& name, const std::string& filenameOrContents = "Hello, world")
        : UnitWSD(name)
        , _filename(DefaultFilename)
        , _countCheckFileInfo(0)
        , _countGetFile(0)
        , _countPutRelative(0)
        , _countPutFile(0)
    {
        LOG_TST("WopiTestServer created for [" << getTestname() << ']');

        // Read the document data and store as string in memory.
        const auto data = helpers::readDataFromFile(filenameOrContents);
        if (!data.empty())
        {
            // That was a filename, set its contents.
            LOG_TST("WopiTestServer created with " << data.size() << " bytes from file ["
                                                   << filenameOrContents << "]");
            _filename = filenameOrContents; // Capture the real filename.
            setFileContent(Util::toString(data));
        }
        else
        {
            // Not a valid filename, assume it's some data.
            LOG_TST("WopiTestServer created with " << filenameOrContents.size()
                                                   << " bytes from data.");
            setFileContent(filenameOrContents);
        }
    }

    std::size_t getCountCheckFileInfo() const { return _countCheckFileInfo; }
    void resetCountCheckFileInfo() { _countCheckFileInfo = 0; }
    std::size_t getCountGetFile() const { return _countGetFile; }
    void resetCountGetFile() { _countGetFile = 0; }
    std::size_t getCountPutRelative() const { return _countPutRelative; }
    void resetCountPutRelative() { _countPutRelative = 0; }
    std::size_t getCountPutFile() const { return _countPutFile; }
    void resetCountPutFile() { _countPutFile = 0; }

    void initWebsocket(const std::string& wopiName)
    {
        Poco::URI wopiURL(helpers::getTestServerURI() + wopiName);

        _wopiSrc.clear();
        Poco::URI::encode(wopiURL.toString(), ":/?", _wopiSrc);

        // This is just a client connection that is used from the tests.
        // It really has nothing to do with this fake WOPI server, exept
        // that it manages it since it is the base of WOPI tests, so
        // it's a common bit of housekeeping that all WOPI tests must do.
        LOG_TST("Connecting test client to LOOL (#" << (_wsList.size() + 1)
                                                    << " connection): /lool/" << _wopiSrc << "/ws");

        // Insert at the front.
        const auto& _ws = _wsList.emplace(
            _wsList.begin(), Util::make_unique<UnitWebSocket>("/lool/" + _wopiSrc + "/ws"));

        assert((*_ws).get());
    }

    void addWebSocket()
    {
        // This is just a client connection that is used from the tests.
        // It really has nothing to do with this fake WOPI server, exept
        // that it manages it since it is the base of WOPI tests, so
        // it's a common bit of housekeeping that all WOPI tests must do.
        LOG_TST("Connecting test client to LOOL (#" << (_wsList.size() + 1)
                                                    << " connection): /lool/" << _wopiSrc << "/ws");

        // Insert at the back.
        const auto& _ws = _wsList.emplace(
            _wsList.end(), Util::make_unique<UnitWebSocket>("/lool/" + _wopiSrc + "/ws"));

        assert((*_ws).get());
    }

    virtual void assertCheckFileInfoRequest(const Poco::Net::HTTPRequest& /*request*/)
    {
    }

    virtual void assertGetFileRequest(const Poco::Net::HTTPRequest& /*request*/)
    {
    }

    /// Assert the PutFile request is valid and optinally return a response.
    virtual std::unique_ptr<http::Response>
    assertPutFileRequest(const Poco::Net::HTTPRequest& /*request*/)
    {
        return nullptr;
    }

    virtual void assertPutRelativeFileRequest(const Poco::Net::HTTPRequest& /*request*/)
    {
    }

    virtual void assertRenameFileRequest(const Poco::Net::HTTPRequest& /*request*/)
    {
    }

    /// Given a URI, returns the filename.
    ///FIXME: this should be remove when we support multiple files properly.
    virtual std::string getFilename(const Poco::URI& uri) const
    {
        std::string filename = extractFilenameFromWopiUri(uri.getPath());

        // Note: This is a fake implementation.
        if (filename == "3")
        {
            // Test '%' in the filename.
            //FIXME: pass this in the URI.
            return "he%llo.txt";
        }

        const auto number = std::stoi(filename);
        if (number >=1 && number <= 9)
        {
            // Fake filename, depends on implicit filename.
            return DefaultFilename;
        }

        // Return the filename given in the URI.
        return filename;
    }

    /// Returns the virtual root-path that we serve.
    static const std::string& getURIRootPath()
    {
        static const std::string RootPath = "/wopi/files/";
        return RootPath;
    }

    /// Given a wopi URI, extracts the filename.
    static std::string extractFilenameFromWopiUri(const std::string& uriPath)
    {
        if (Util::startsWith(uriPath, getURIRootPath()))
        {
            const auto first = getURIRootPath().size();
            const auto it = uriPath.find_first_of('/', first);
            return uriPath.substr(first, it);
        }

        return std::string();
    }

    /// Returns true iff @uriPath is a Wopi path but not to the contents.
    static bool isWopiInfoRequest(const std::string& uriPath)
    {
        return Util::startsWith(uriPath, getURIRootPath()) && !Util::endsWith(uriPath, "/contents");
    }

    /// Returns true iff @uriPath is a Wopi path to the contents of a file.
    static bool isWopiContentRequest(const std::string& uriPath)
    {
        return Util::startsWith(uriPath, getURIRootPath()) && Util::endsWith(uriPath, "/contents");
    }

    void configure(Poco::Util::LayeredConfiguration& config) override
    {
        UnitWSD::configure(config);
        // we're still internally confused as to https vs. http in places.
        config.setBool("storage.ssl.as_scheme", false);
    }

    /// Handles WOPI CheckFileInfo requests.
    virtual bool handleCheckFileInfoRequest(const Poco::Net::HTTPRequest& request,
                                            std::shared_ptr<StreamSocket>& socket)
    {
        const Poco::URI uriReq(request.getURI());

        Poco::JSON::Object::Ptr fileInfo = new Poco::JSON::Object();
        fileInfo->set("BaseFileName", getFilename(uriReq));
        fileInfo->set("Size", getFileContent().size());
        fileInfo->set("Version", "1.0");
        fileInfo->set("OwnerId", "test");
        fileInfo->set("UserId", "test");
        fileInfo->set("UserFriendlyName", "test");
        fileInfo->set("UserCanWrite", "true");
        fileInfo->set("PostMessageOrigin", "localhost");
        fileInfo->set("LastModifiedTime",
                      Util::getIso8601FracformatTime(getFileLastModifiedTime()));
        fileInfo->set("EnableOwnerTermination", "true");

        std::ostringstream jsonStream;
        fileInfo->stringify(jsonStream);

        http::Response httpResponse(http::StatusLine(200));
        httpResponse.set("Last-Modified", Util::getHttpTime(getFileLastModifiedTime()));
        httpResponse.setBody(jsonStream.str(), "application/json; charset=utf-8");
        socket->sendAndShutdown(httpResponse);

        return true;
    }

    virtual bool handleGetFileRequest(const Poco::Net::HTTPRequest&,
                                      std::shared_ptr<StreamSocket>& socket)
    {
        http::Response httpResponse(http::StatusLine(200));
        httpResponse.set("Last-Modified", Util::getHttpTime(getFileLastModifiedTime()));
        httpResponse.setBody(getFileContent(), "application/octet-stream");
        socket->sendAndShutdown(httpResponse);

        return true;
    }

    virtual bool handleHttpGetRequest(const Poco::Net::HTTPRequest& request,
                                      std::shared_ptr<StreamSocket>& socket)
    {
        LOG_ASSERT_MSG(request.getMethod() == "GET", "Expect an HTTP GET request");

        const Poco::URI uriReq(request.getURI());

        if (isWopiInfoRequest(uriReq.getPath())) // CheckFileInfo
        {
            ++_countCheckFileInfo;
            LOG_TST("Fake wopi host request, handling CheckFileInfo (#"
                    << _countCheckFileInfo << "): " << uriReq.getPath());

            assertCheckFileInfoRequest(request);

            return handleCheckFileInfoRequest(request, socket);
        }
        else if (isWopiContentRequest(uriReq.getPath())) // GetFile
        {
            ++_countGetFile;
            LOG_TST("Fake wopi host request, handling GetFile (#" << _countGetFile
                                                                  << "): " << uriReq.getPath());

            assertGetFileRequest(request);

            return handleGetFileRequest(request, socket);
        }

        return false;
    }

    virtual bool handleHttpPostRequest(const Poco::Net::HTTPRequest& request,
                                       Poco::MemoryInputStream& message,
                                       std::shared_ptr<StreamSocket>& socket)
    {
        LOG_ASSERT_MSG(request.getMethod() == "POST", "Expect an HTTP POST request");

        const Poco::URI uriReq(request.getURI());
        if (isWopiInfoRequest(uriReq.getPath()))
        {
            ++_countPutRelative;
            LOG_TST("Fake wopi host request, handling PutRelativeFile (#"
                    << _countPutRelative << "): " << uriReq.getPath());

            const std::string wopiURL =
                helpers::getTestServerURI() +
                "/something wopi/files/1?access_token=anything&reuse_cookies=cook=well";

            std::string content;
            if (request.get("X-WOPI-Override") == std::string("PUT_RELATIVE"))
            {
                LOK_ASSERT_EQUAL(std::string("PUT_RELATIVE"), request.get("X-WOPI-Override"));
                assertPutRelativeFileRequest(request);
                content = "{ \"Name\":\"hello world%1.pdf\", \"Url\":\"" + wopiURL + "\" }";
            }
            else
            {
                // rename file; response should be the file name without the url and the extension
                LOK_ASSERT_EQUAL(std::string("RENAME_FILE"), request.get("X-WOPI-Override"));
                assertRenameFileRequest(request);
                content = "{ \"Name\":\"hello\", \"Url\":\"" + wopiURL + "\" }";
            }

            http::Response httpResponse(http::StatusLine(200));
            httpResponse.set("Last-Modified", Util::getHttpTime(getFileLastModifiedTime()));
            httpResponse.setBody(content, "application/json; charset=utf-8");
            socket->sendAndShutdown(httpResponse);

            return true;
        }
        else if (isWopiContentRequest(uriReq.getPath()))
        {
            ++_countPutFile;
            LOG_TST("Fake wopi host request, handling PutFile (#" << _countPutFile
                                                                  << "): " << uriReq.getPath());

            const std::string wopiTimestamp = request.get("X-LOOL-WOPI-Timestamp", std::string());
            if (!wopiTimestamp.empty())
            {
                const std::string fileModifiedTime =
                    Util::getIso8601FracformatTime(getFileLastModifiedTime());
                if (wopiTimestamp != fileModifiedTime)
                {
                    http::Response httpResponse(http::StatusLine(409));
                    httpResponse.setBody(
                        "{\"LOOLStatusCode\":" +
                        std::to_string(static_cast<int>(LOOLStatusCode::DocChanged)) + '}');
                    socket->sendAndShutdown(httpResponse);
                    return true;
                }
            }

            std::unique_ptr<http::Response> response = assertPutFileRequest(request);
            if (!response || response->statusLine().statusCategory() ==
                                 http::StatusLine::StatusCodeClass::Successful)
            {
                const std::streamsize size = request.getContentLength();
                LOG_TST("Fake wopi host writing document contents in storage (" << size
                                                                                << "bytes)");
                std::vector<char> buffer(size);
                message.read(buffer.data(), size);
                setFileContent(Util::toString(buffer));
            }

            if (response)
            {
                LOG_TST("Fake wopi host response to POST "
                        << uriReq.getPath() << ": " << response->statusLine().statusCode() << ' '
                        << response->statusLine().reasonPhrase());
                socket->sendAndShutdown(*response);
            }
            else
            {
                // By default we return success.
                const std::string body = "{\"LastModifiedTime\": \"" +
                                         Util::getIso8601FracformatTime(getFileLastModifiedTime()) +
                                         "\" }";
                LOG_TST("Fake wopi host (default) response to POST " << uriReq.getPath()
                                                                     << ": 200 OK " << body);
                http::Response httpResponse(http::StatusLine(200));
                httpResponse.setBody(body, "application/json; charset=utf-8");
                socket->sendAndShutdown(httpResponse);
            }

            return true;
        }

        return false;
    }

    /// Here we act as a WOPI server, so that we have a server that responds to
    /// the wopi requests without additional expensive setup.
    bool handleHttpRequest(const Poco::Net::HTTPRequest& request, Poco::MemoryInputStream& message,
                           std::shared_ptr<StreamSocket>& socket) override
    {
        Poco::URI uriReq(request.getURI());

        {
            std::ostringstream oss;
            oss << "Fake wopi host " << request.getMethod() << " request URI [" << uriReq.toString()
                << "]:\n";
            for (const auto& pair : request)
            {
                oss << '\t' << pair.first << ": " << pair.second << " / ";
            }

            LOG_TST(oss.str());
        }

        if (request.getMethod() == "GET")
        {
            return handleHttpGetRequest(request, socket);
        }
        else if (request.getMethod() == "POST")
        {
            return handleHttpPostRequest(request, message, socket);
        }
        else if (!Util::startsWith(uriReq.getPath(), "/lool/")) // Skip requests to the websrv.
        {
            // Complain if we are expected to handle something that we don't.
            LOG_TST("ERROR: Fake wopi host request, cannot handle request: " << uriReq.getPath());
        }

        return false;
    }

};

/// Send a command message to WSD from a WopiTestServer on the given connection.
#define WSD_CMD_BY_CONNECTION_INDEX(INDEX, MSG)                                                    \
    do                                                                                             \
    {                                                                                              \
        LOG_TST("Sending from #" << INDEX << ": " << MSG);                                         \
        helpers::sendTextFrame(*getWsAt(INDEX)->getLOOLWebSocket(), MSG, getTestname());           \
        SocketPoll::wakeupWorld();                                                                 \
    } while (false)

/// Send a command message to WSD from a WopiTestServer on the primary connection.
#define WSD_CMD(MSG) WSD_CMD_BY_CONNECTION_INDEX(0, MSG)

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
