/*
Copyright 2008 - 2009 Sun Microsystems, Inc.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 2 of the License.

There are special exceptions to the terms and conditions of the GPL
as it is applied to this software. View the full text of the
exception in file EXCEPTIONS-CONNECTOR-C++ in the directory of this
software distribution.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <cppconn/warning.h>

#include "connection.h"
#include <sstream>

namespace testsuite
{
namespace classes
{

void connection::getClientInfo()
{
  logMsg("connection::getClientInfo() - MySQL_Connection::getClientInfo()");
  try
  {
    std::string client_info;
    std::string ret;

    ret=con->getClientInfo(client_info);
    if (ret != "cppconn")
      FAIL("Expecting 'cppconn' got '" + ret + "'.");

    if (!client_info.empty())
      FAIL("Expecting parameter to be unchanged but it seems to have been modified.");

  } catch (sql::SQLException &e)
  {
    logErr(e.what());
    logErr("SQLState: " + e.getSQLState());
    FAIL(e.what());
  }
}

void connection::getNoWarningsOnNewLine()
{
  logMsg("connection::getNoWarningsOnNewLine() - MySQL_Connection::getWarnings()");
  try
  {
    const sql::SQLWarning* warning;

    warning=con->getWarnings();
    if (warning != NULL)
      FAIL("There should be no warnings on the default connection");

  } catch (sql::SQLException &e)
  {
    logErr(e.what());
    logErr("SQLState: " + e.getSQLState());
    FAIL(e.what());
  }
}

void connection::getNoWarningsAfterClear()
{
  logMsg("connection::getNoWarningsAfterClear() - MySQL_Connection::getWarnings()");
  try
  {
    const sql::SQLWarning* warning;

    /* TODO: pointless test as there is no warning before running clearWarnings() */
    con->clearWarnings();
    warning=con->getWarnings();
    if (warning != NULL)
      FAIL("There should be no warnings on the default connection");

  } catch (sql::SQLException &e)
  {
    logErr(e.what());
    logErr("SQLState: " + e.getSQLState());
    FAIL(e.what());
  }
}

void connection::checkClosed()
{
  logMsg("connection::checkClosed - MySQL_Connection::close, isClosed() and internal check_closed()");
  try
  {
    if (con->isClosed())
      FAIL("Connection should not be reported as closed");

    con->close();

    if (!con->isClosed())
      FAIL("Connection should be closed");

    try
    {
      con->rollback();
    } catch (sql::SQLException &e)
    {
      std::string reason(exceptionIsOK(e, "HY000", 1000));
      if (!reason.empty())
        FAIL(reason);
    }


  } catch (sql::SQLException &e)
  {
    logErr(e.what());
    logErr("SQLState: " + e.getSQLState());
    FAIL(e.what());
  }
}

void connection::connectUsingMap()
{
  logMsg("connection::connectUsingMap - using map to pass connection parameters");

  try
  {
    std::map<std::string, sql::ConnectPropertyVal> connection_properties;

    {
      sql::ConnectPropertyVal tmp;
      /* url comes from the unit testing framework */
      tmp.str.val=url.c_str();
      tmp.str.len=url.length();
      connection_properties[std::string("hostName")]=tmp;
    }

    {
      sql::ConnectPropertyVal tmp;
      /* user comes from the unit testing framework */
      tmp.str.val=user.c_str();
      tmp.str.len=user.length();
      connection_properties[std::string("userName")]=tmp;
    }

    {
      sql::ConnectPropertyVal tmp;
      tmp.str.val=passwd.c_str();
      tmp.str.len=passwd.length();
      connection_properties[std::string("password")]=tmp;
    }

    created_objects.clear();
    con.reset(driver->connect(connection_properties));

    /*     
    The property map now contains the minimum required entries and it works.
    Lets play with so-to-say optional entries.
     */

    /* 1) Port */
    connection_properties.erase("port");
    {
      int port= -1;
      if (url.compare(0, sizeof ("tcp://") - 1, "tcp://") == 0)
      {
        size_t port_pos;
        port_pos=url.find_last_of(":", std::string::npos);
        if (port_pos != std::string::npos)
          port=atoi(url.substr(port_pos + 1, std::string::npos).c_str());

        if (port == -1)
        {
          /* The user is using TCP/IP and default port 3306 */
          sql::ConnectPropertyVal tmp;
          tmp.lval=(long long) port;
          try
          {
            created_objects.clear();
            con.reset(driver->connect(connection_properties));
            FAIL("Connect works with invalid port of -1");
          } catch (sql::SQLException &e)
          {
            std::string reason(exceptionIsOK(e, "HY000", 2003));
            logErr(reason);
          }
        } else
        {
          /* The user is using TCP/IP and has specified the port.
           A port setting shall NOT overrule the setting from the URL */
          port= -1;
          sql::ConnectPropertyVal tmp;
          tmp.lval=(long long) port;
          try
          {
            created_objects.clear();
            con.reset(driver->connect(connection_properties));
          } catch (sql::SQLException &e)
          {
            FAIL("URL shall overrule port setting");
          }
        }

      } else
      {
        /* We must be using a socket connection - all port settings shall be ignored */
        sql::ConnectPropertyVal tmp;
        tmp.lval=(long long) port;
        connection_properties[std::string("port")]=tmp;
        /* Must not throw an exception */
        try
        {
          created_objects.clear();
          con.reset(driver->connect(connection_properties));
        } catch (sql::SQLException &e)
        {
          FAIL("Port setting should be ignored, using Unix socket!");
        }
      }
    }
    connection_properties.erase("port");

    /* 2) Socket */
    connection_properties.erase("socket");
    {
      std::string socket("");
#ifndef CPPDBC_WIN32

      if (url.compare(0, sizeof ("unix://") - 1, "unix://") == 0)
      {
        // Unix socket connection
        socket="I hope this is invalid";
        try
        {
          created_objects.clear();
          con.reset(driver->connect(connection_properties));
        } catch (sql::SQLException &e)
        {
          FAIL("Socket setting should be ignored, socket is part of the URL");
        }

      } else
#endif
        if (url.compare(0, sizeof ("tcp://") - 1, "tcp://") != 0)
      {
        // Unix socket connection
        socket="I hope this is invalid";
        try
        {
          created_objects.clear();
          con.reset(driver->connect(connection_properties));
        } catch (sql::SQLException &e)
        {
          FAIL("Socket setting should be ignored because its a TCP/IP connection");
        }
      }
    }

  } catch (sql::SQLException &e)
  {
    logErr(e.what());
    logErr("SQLState: " + e.getSQLState());
    FAIL(e.what());
  }


}

std::string connection::exceptionIsOK(sql::SQLException & e)
{
  return exceptionIsOK(e, "HY000", 0);
}

std::string connection::exceptionIsOK(sql::SQLException &e, const std::string& sql_state, int errno)
{

  std::stringstream reason;
  reason.str("");

  std::string what(e.what());
  if (what.empty())
  {
    reason << "Exception must not have an empty message.";
    logMsg(reason.str());
    return reason.str();
  }

  if (e.getErrorCode() != errno)
  {
    reason << "Expecting error code '" << errno << "' got '" << e.getErrorCode() << "'";
    logMsg(reason.str());
    return reason.str();
  }

  if (e.getSQLState() != sql_state)
  {
    reason << "Expecting sqlstate '" << sql_state << "' got '" << e.getSQLState() << "'";
    logMsg(reason.str());
    return reason.str();
  }

  return reason.str();
}

} /* namespace connection */
} /* namespace testsuite */
