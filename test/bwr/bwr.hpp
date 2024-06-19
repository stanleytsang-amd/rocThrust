// Copyright (c) 2024 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include "sqlite3.h"

typedef size_t default_hash_type;

struct hash_input
{
    hash_input(const thrust_params& params, const bool use_ivector)
    {
        precision = params.precision;

        if(use_ivector)
        {
            vec_length   = params.ilength();
            vec_sizes    = params.ivector_sizes();
            vec_stride   = params.istride;
            vec_dist     = params.idist;
            vec_size     = params.isize;
            vec_type     = params.itype;
        }
        else
        {
            vec_length   = params.olength();
            vec_sizes    = params.ovector_sizes();
            vec_stride   = params.ostride;
            vec_dist     = params.odist;
            vec_size     = params.osize;
            vec_type     = params.otype;
        }

        length = params.length;

        nbatch = params.nbatch;

        token = params.token();
    }

    ~hash_input() {}

    //thrust_precision precision;

    std::vector<size_t> vec_length;
    std::vector<size_t> vec_sizes;
    std::vector<size_t> vec_stride;
    size_t              vec_dist;
    std::vector<size_t> vec_size;
    thrust_array_type   vec_type;

    std::vector<size_t> length;

    size_t nbatch;

    std::string token;
};

class BWRDatabase
{
public:
    BWRDatabase(std::string db_path)
        : ret(SQLITE_OK)
        , db_connection(nullptr)
        , begin_stmt(nullptr)
        , end_stmt(nullptr)
        , insert_stmt(nullptr)
        , match_stmt(nullptr)
    {
        ret = sqlite3_open(db_path.c_str(), &db_connection);
        if(ret != SQLITE_OK)
            throw std::runtime_error(std::string("Cannot open repro-db: ") + db_path);

        // Access to a database file may occur in parallel.
        // Increase default sqlite timeout, so diferent process
        // can wait for one another.
        sqlite3_busy_timeout(db_connection, 30000);

        // Set sqlite3 engine to WAL mode to avoid potential deadlocks with multiple
        // concurrent processes (if a deadlock occurs, the busy timeout is not honored).
        ret = sqlite3_exec(db_connection, "PRAGMA journal_mode = WAL", nullptr, nullptr, nullptr);
        if(ret != SQLITE_OK)
            throw std::runtime_error("Error setting WAL mode: "
                                     + std::string(sqlite3_errmsg(db_connection)));

        ret = sqlite3_exec(db_connection,
                           rocthrust_test_run<>::get_create_rocthrust_test_run_sql().c_str(),
                           nullptr,
                           nullptr,
                           nullptr);
        if(ret != SQLITE_OK)
            throw std::runtime_error("Error creating table: "
                                     + std::string(sqlite3_errmsg(db_connection)));

        prepare_begin_end_stmts();
        prepare_match_stmt();
        prepare_insert_stmt();
    }

    ~BWRDatabase()
    {
        sqlite3_finalize(begin_stmt);
        sqlite3_finalize(end_stmt);
        sqlite3_finalize(match_stmt);
        sqlite3_finalize(insert_stmt);
        sqlite3_close(db_connection);
    }

    template <typename Tint>
    void check_hash_valid(const hash_output<Tint>& ibuffer_hash,
                          const hash_output<Tint>& obuffer_hash,
                          const std::string&       token,
                          bool&                    hash_entry_found,
                          bool&                    hash_valid)
    {
        hash_valid = true;

        auto test_run = get_rocthrust_test_run<Tint>(ibuffer_hash, obuffer_hash, token);

        begin_transaction();

        hash_entry_found = check_match(&test_run);

        if(hash_entry_found)
            hash_valid = (test_run.ibuffer_hash_real == ibuffer_hash.buffer_real
                          && test_run.ibuffer_hash_imag == ibuffer_hash.buffer_imag
                          && test_run.obuffer_hash_real == obuffer_hash.buffer_real
                          && test_run.obuffer_hash_imag == obuffer_hash.buffer_imag)
                             ? true
                             : false;
        else
            insert(&test_run);

        end_transaction();
    }

private:
    void prepare_begin_end_stmts()
    {
        auto begin_sql = std::string("BEGIN TRANSACTION;");

        ret = sqlite3_prepare_v2(db_connection, begin_sql.c_str(), -1, &begin_stmt, nullptr);
        if(ret != SQLITE_OK)
            throw std::runtime_error("Cannot prepare begin statement: "
                                     + std::string(sqlite3_errmsg(db_connection)));

        auto end_sql = std::string("END TRANSACTION;");

        ret = sqlite3_prepare_v2(db_connection, end_sql.c_str(), -1, &end_stmt, nullptr);
        if(ret != SQLITE_OK)
            throw std::runtime_error("Cannot prepare end statement: "
                                     + std::string(sqlite3_errmsg(db_connection)));
    }

    void prepare_match_stmt()
    {
        auto match_sql = rocthrust_test_run<>::get_match_sql();

        ret = sqlite3_prepare_v2(db_connection, match_sql.c_str(), -1, &match_stmt, nullptr);
        if(ret != SQLITE_OK)
            throw std::runtime_error("Cannot prepare match statement: "
                                     + std::string(sqlite3_errmsg(db_connection)));
    }

    void prepare_insert_stmt()
    {
        auto insert_sql = rocthrust_test_run<>::get_insert_sql();

        ret = sqlite3_prepare_v2(db_connection, insert_sql.c_str(), -1, &insert_stmt, nullptr);
        if(ret != SQLITE_OK)
            throw std::runtime_error("Cannot prepare insert statement: "
                                     + std::string(sqlite3_errmsg(db_connection)));
    }

    void begin_transaction()
    {
        ret = sqlite3_step(begin_stmt);
        if(ret != SQLITE_DONE)
            throw std::runtime_error(std::string("Error executing begin statement: ")
                                     + std::string(sqlite3_errmsg(db_connection)));
    }

    void end_transaction()
    {
        ret = sqlite3_step(end_stmt);
        if(ret != SQLITE_DONE)
            throw std::runtime_error(std::string("Error executing end statement: ")
                                     + std::string(sqlite3_errmsg(db_connection)));
    }

    template <typename Tint>
    bool check_match(rocthrust_test_run<Tint>* entry)
    {
        sqlite3_reset(match_stmt);

        entry->bind_match_statement(match_stmt);

        size_t match_count = 0;
        while((ret = sqlite3_step(match_stmt)) == SQLITE_ROW)
        {
            entry->update(match_stmt);
            match_count++;
        }

        // There can only be one result in this query
        if(match_count > 1)
            throw std::runtime_error("Corrupted database");

        if(ret != SQLITE_DONE)
            throw std::runtime_error(std::string("Error executing select statement: ")
                                     + std::string(sqlite3_errmsg(db_connection)));

        return match_count;
    }

    template <typename Tint>
    void insert(rocthrust_test_run<Tint>* entry)
    {
        sqlite3_reset(insert_stmt);

        entry->bind_insert_statement(insert_stmt);

        ret = sqlite3_step(insert_stmt);
        if(ret != SQLITE_DONE)
            throw std::runtime_error(std::string("Error executing insert statement: ")
                                     + std::string(sqlite3_errmsg(db_connection)));
    }

    int           ret;
    sqlite3*      db_connection;
    sqlite3_stmt* begin_stmt;
    sqlite3_stmt* end_stmt;
    sqlite3_stmt* insert_stmt;
    sqlite3_stmt* match_stmt;    
};