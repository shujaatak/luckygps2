/*****************************************************************************
 * 
 * This file is part of Mapnik (c++ mapping toolkit)
 *
 * Copyright (C) 2007 Artem Pavlenko
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *****************************************************************************/
//$Id$

#ifndef SQLITE_TYPES_HPP
#define SQLITE_TYPES_HPP

// mapnik
#include <mapnik/datasource.hpp>

// boost
#include <boost/shared_ptr.hpp>

// sqlite
extern "C" {
  #include <sqlite3.h>
}


class sqlite_resultset
{
public:

    sqlite_resultset (sqlite3_stmt* stmt)
        : stmt_(stmt)
    {
    }

    ~sqlite_resultset ()
    {
        if (stmt_)
            sqlite3_finalize (stmt_);
    }

    bool is_valid ()
    {
        return stmt_ != 0;
    }

    bool step_next ()
    {
        int status = sqlite3_step (stmt_);
        if (status != SQLITE_ROW && status != SQLITE_DONE) {
            std::ostringstream s;
            s << "SQLite Plugin: retrieving next row failed";
            std::string msg(sqlite3_errmsg(sqlite3_db_handle(stmt_)));
            if (msg != "unknown error") s << ": " << msg;
            throw mapnik::datasource_exception(s.str());
        }
        return status == SQLITE_ROW;
    }

    int column_count ()
    {
        return sqlite3_column_count (stmt_);
    }

    int column_type (int col)
    {
        return sqlite3_column_type (stmt_, col);
    }
    
    const char* column_name (int col)
    {
        return sqlite3_column_name (stmt_, col);
    }

    bool column_isnull (int col)
    {
        return sqlite3_column_type (stmt_, col) == SQLITE_NULL;
    }

    int column_integer (int col)
    {
        return sqlite3_column_int (stmt_, col);
    }

    int column_integer64 (int col)
    {
        return sqlite3_column_int64 (stmt_, col);
    }

    double column_double (int col)
    {
        return sqlite3_column_double (stmt_, col);
    }

    const char* column_text (int col, int& len)
    {
        const char* text = (const char*) sqlite3_column_text (stmt_, col);
        len = sqlite3_column_bytes (stmt_, col);
        return text;
    }

    const char* column_text (int col)
    {
        return (const char*) sqlite3_column_text (stmt_, col);
    }

    const void* column_blob (int col, int& bytes)
    {
        const char* blob = (const char*) sqlite3_column_blob (stmt_, col);
        bytes = sqlite3_column_bytes (stmt_, col);
        return blob;
    }

    sqlite3_stmt* get_statement()
    {
        return stmt_;
    }

private:

    sqlite3_stmt* stmt_;
};



class sqlite_connection
{
public:

    sqlite_connection (const std::string& file)
        : db_(0)
    {
        // sqlite3_open_v2 is available earlier but 
        // shared cache not available until >= 3.6.18
        #if SQLITE_VERSION_NUMBER >= 3006018
        int rc = sqlite3_enable_shared_cache(1);
        if (rc != SQLITE_OK)
        {
           throw mapnik::datasource_exception (sqlite3_errmsg (db_));
        }
        
        int mode = SQLITE_OPEN_READWRITE | SQLITE_OPEN_NOMUTEX | SQLITE_OPEN_SHAREDCACHE;
        if (sqlite3_open_v2 (file.c_str(), &db_, mode, NULL))
        #else
        #warning "Mapnik's sqlite plugin is compiling against an version of sqlite older than 3.6.18 which may make rendering slow..."
        if (sqlite3_open (file.c_str(), &db_))
        #endif
        {
            std::ostringstream s;
            s << "Sqlite Plugin: ";
            throw mapnik::datasource_exception (sqlite3_errmsg (db_));
        }
        //sqlite3_enable_load_extension(db_, 1);
    }

    ~sqlite_connection ()
    {
        if (db_)
            sqlite3_close (db_);
    }

    void throw_sqlite_error(const std::string& sql)
    {
      std::ostringstream s;
      s << "Sqlite Plugin: ";
      if (db_)
          s << "'" << sqlite3_errmsg(db_) << "'";
      else
          s << "unknown error, lost connection";
      
      s << "\nFull sql was: '" <<  sql << "'\n";
      throw mapnik::datasource_exception( s.str() ); 
    }
    
    sqlite_resultset* execute_query(const std::string& sql)
    {
        sqlite3_stmt* stmt = 0;

        int rc = sqlite3_prepare_v2 (db_, sql.c_str(), -1, &stmt, 0);
        if (rc != SQLITE_OK)
        {
           throw_sqlite_error(sql);
        }

        return new sqlite_resultset (stmt);
    }
  
    void execute(const std::string& sql)
    {
        int rc=sqlite3_exec(db_, sql.c_str(), 0, 0, 0);
        if (rc != SQLITE_OK)
        {
            throw_sqlite_error(sql);
        }
    }

    int execute_with_code(const std::string& sql)
    {
        int rc=sqlite3_exec(db_, sql.c_str(), 0, 0, 0);
        return rc;
    }
   
    sqlite3* operator*()
    {
        return db_;
    }

private:

    sqlite3* db_;
};

#endif //SQLITE_TYPES_HPP

