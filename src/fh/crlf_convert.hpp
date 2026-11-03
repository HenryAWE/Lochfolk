/**
 * @file fh/crlf_convert.hpp
 * @brief Shared CRLF to LF translation used by the CRLF handle and streambuf
 */

#ifndef LOCHFOLK_FH_CRLF_CONVERT_HPP
#define LOCHFOLK_FH_CRLF_CONVERT_HPP

#pragma once

#include <cassert>
#include <cstddef>
#include <span>
#include <vector>

namespace lochfolk
{
namespace detail
{
    inline constexpr std::byte cr = static_cast<std::byte>('\r');
    inline constexpr std::byte lf = static_cast<std::byte>('\n');

    /**
     * @brief Translates CRLF sequences in @p raw to LF
     *
     * The translation reads from @p raw and writes to a distinct @p out
     * buffer, so it never overwrites bytes that have not been read yet. A CR
     * that ends a chunk is held in @p pending_cr and resolved by the next
     * chunk; an empty @p raw flushes it as a lone CR (end of file).
     *
     * @param raw Input bytes; may be empty to flush a held CR
     * @param out Output buffer; must have room for `raw.size() + 1` bytes
     * @param pending_cr CR held back from the previous chunk; updated in place
     * @param pair_positions If not null, receives the output index of every LF
     *        that replaced a CRLF pair
     *
     * @return Number of bytes written to @p out
     */
    inline std::size_t convert_crlf(
        std::span<const std::byte> raw,
        std::span<std::byte> out,
        bool& pending_cr,
        std::vector<std::size_t>* pair_positions = nullptr
    )
    {
        assert(out.size() >= raw.size() + 1);

        std::size_t written = 0;
        std::size_t i = 0;

        if(pending_cr)
        {
            pending_cr = false;
            if(!raw.empty() && raw[0] == lf)
            {
                // The LF completes a pair whose CR ended the previous chunk.
                if(pair_positions)
                    pair_positions->push_back(written);
                out[written++] = lf;
                i = 1;
            }
            else
            {
                // The held CR was not followed by an LF.
                out[written++] = cr;
            }
        }

        for(; i < raw.size(); ++i)
        {
            if(raw[i] != cr)
            {
                out[written++] = raw[i];
                continue;
            }

            if(i + 1 < raw.size())
            {
                if(raw[i + 1] == lf)
                {
                    if(pair_positions)
                        pair_positions->push_back(written);
                    out[written++] = lf;
                    ++i;
                }
                else
                {
                    out[written++] = cr;
                }
            }
            else
            {
                // The next byte is in the following chunk.
                pending_cr = true;
            }
        }

        return written;
    }
} // namespace detail
} // namespace lochfolk

#endif
