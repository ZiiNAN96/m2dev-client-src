//--------------------------------------------------------------------------------------
// File: DDSTextureLoader9.h
//
// Functions for loading a DDS texture and creating a Direct3D runtime resource for it
//
// Note these functions are useful as a light-weight runtime loader for DDS files. For
// a full-featured DDS file reader, writer, and texture processing pipeline see
// the 'Texconv' sample and the 'DirectXTex' library.
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248926
//--------------------------------------------------------------------------------------

#pragma once

#ifndef DIRECT3D_VERSION
#define DIRECT3D_VERSION 0x900
#endif

#include <d3d9.h>

#include <cstddef>
#include <cstdint>
#include <array>


namespace DirectX
{
    // CPU-only view of the same legacy DDS layout used by the D3D9 loader.
    // Views borrow the input buffer. No texture allocation, decoding or mip generation.
    struct DDS2DView
    {
        struct Mip { const uint8_t* data = nullptr; size_t size = 0, rowPitch = 0; };
        uint32_t width = 0, height = 0, mipCount = 0;
        D3DFORMAT format = D3DFMT_UNKNOWN;
        std::array<Mip,14> mips{};
    };
    HRESULT GetDDS2DView(const uint8_t* data, size_t size, DDS2DView& result) noexcept;

    // Standard version
    HRESULT CreateDDSTextureFromMemory(
        _In_ LPDIRECT3DDEVICE9 d3dDevice,
        _In_reads_bytes_(ddsDataSize) const uint8_t* ddsData,
        _In_ size_t ddsDataSize,
        _Outptr_ LPDIRECT3DBASETEXTURE9* texture,
        bool generateMipsIfMissing = false) noexcept;

    HRESULT CreateDDSTextureFromFile(
        _In_ LPDIRECT3DDEVICE9 d3dDevice,
        _In_z_ const wchar_t* fileName,
        _Outptr_ LPDIRECT3DBASETEXTURE9* texture,
        bool generateMipsIfMissing = false) noexcept;

    // Extended version
    HRESULT CreateDDSTextureFromMemoryEx(
        _In_ LPDIRECT3DDEVICE9 d3dDevice,
        _In_reads_bytes_(ddsDataSize) const uint8_t* ddsData,
        _In_ size_t ddsDataSize,
        _In_ DWORD usage,
        _In_ D3DPOOL pool,
        bool generateMipsIfMissing,
        _Outptr_ LPDIRECT3DBASETEXTURE9* texture) noexcept;

    HRESULT CreateDDSTextureFromFileEx(
        _In_ LPDIRECT3DDEVICE9 d3dDevice,
        _In_z_ const wchar_t* fileName,
        _In_ DWORD usage,
        _In_ D3DPOOL pool,
        bool generateMipsIfMissing,
        _Outptr_ LPDIRECT3DBASETEXTURE9* texture) noexcept;

    // Type-specific standard versions
    HRESULT CreateDDSTextureFromMemory(
        _In_ LPDIRECT3DDEVICE9 d3dDevice,
        _In_reads_bytes_(ddsDataSize) const uint8_t* ddsData,
        _In_ size_t ddsDataSize,
        _Outptr_ LPDIRECT3DTEXTURE9* texture,
        bool generateMipsIfMissing = false) noexcept;

    HRESULT CreateDDSTextureFromFile(
        _In_ LPDIRECT3DDEVICE9 d3dDevice,
        _In_z_ const wchar_t* fileName,
        _Outptr_ LPDIRECT3DTEXTURE9* texture,
        bool generateMipsIfMissing = false) noexcept;

    HRESULT CreateDDSTextureFromMemory(
        _In_ LPDIRECT3DDEVICE9 d3dDevice,
        _In_reads_bytes_(ddsDataSize) const uint8_t* ddsData,
        _In_ size_t ddsDataSize,
        _Outptr_ LPDIRECT3DCUBETEXTURE9* texture) noexcept;

    HRESULT CreateDDSTextureFromFile(
        _In_ LPDIRECT3DDEVICE9 d3dDevice,
        _In_z_ const wchar_t* fileName,
        _Outptr_ LPDIRECT3DCUBETEXTURE9* texture) noexcept;

    HRESULT CreateDDSTextureFromMemory(
        _In_ LPDIRECT3DDEVICE9 d3dDevice,
        _In_reads_bytes_(ddsDataSize) const uint8_t* ddsData,
        _In_ size_t ddsDataSize,
        _Outptr_ LPDIRECT3DVOLUMETEXTURE9* texture) noexcept;

    HRESULT CreateDDSTextureFromFile(
        _In_ LPDIRECT3DDEVICE9 d3dDevice,
        _In_z_ const wchar_t* fileName,
        _Outptr_ LPDIRECT3DVOLUMETEXTURE9* texture) noexcept;

    // Type-specific extended versions
    HRESULT CreateDDSTextureFromMemoryEx(
        _In_ LPDIRECT3DDEVICE9 d3dDevice,
        _In_reads_bytes_(ddsDataSize) const uint8_t* ddsData,
        _In_ size_t ddsDataSize,
        _In_ DWORD usage,
        _In_ D3DPOOL pool,
        bool generateMipsIfMissing,
        _Outptr_ LPDIRECT3DTEXTURE9* texture) noexcept;

    HRESULT CreateDDSTextureFromFileEx(
        _In_ LPDIRECT3DDEVICE9 d3dDevice,
        _In_z_ const wchar_t* fileName,
        _In_ DWORD usage,
        _In_ D3DPOOL pool,
        bool generateMipsIfMissing,
        _Outptr_ LPDIRECT3DTEXTURE9* texture) noexcept;

    HRESULT CreateDDSTextureFromMemoryEx(
        _In_ LPDIRECT3DDEVICE9 d3dDevice,
        _In_reads_bytes_(ddsDataSize) const uint8_t* ddsData,
        _In_ size_t ddsDataSize,
        _In_ DWORD usage,
        _In_ D3DPOOL pool,
        _Outptr_ LPDIRECT3DCUBETEXTURE9* texture) noexcept;

    HRESULT CreateDDSTextureFromFileEx(
        _In_ LPDIRECT3DDEVICE9 d3dDevice,
        _In_z_ const wchar_t* fileName,
        _In_ DWORD usage,
        _In_ D3DPOOL pool,
        _Outptr_ LPDIRECT3DCUBETEXTURE9* texture) noexcept;

    HRESULT CreateDDSTextureFromMemoryEx(
        _In_ LPDIRECT3DDEVICE9 d3dDevice,
        _In_reads_bytes_(ddsDataSize) const uint8_t* ddsData,
        _In_ size_t ddsDataSize,
        _In_ DWORD usage,
        _In_ D3DPOOL pool,
        _Outptr_ LPDIRECT3DVOLUMETEXTURE9* texture) noexcept;

    HRESULT CreateDDSTextureFromFileEx(
        _In_ LPDIRECT3DDEVICE9 d3dDevice,
        _In_z_ const wchar_t* fileName,
        _In_ DWORD usage,
        _In_ D3DPOOL pool,
        _Outptr_ LPDIRECT3DVOLUMETEXTURE9* texture) noexcept;
}
