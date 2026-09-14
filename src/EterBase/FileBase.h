#ifndef __INC_ETERBASE_FILEBASE_H__
#define __INC_ETERBASE_FILEBASE_H__

#include "Platform/PlatformFilesystem.h"

#include <cstdint>

class CFileBase
{
	public:
		enum EFileMode
		{
			FILEMODE_READ = (1 << 0),
			FILEMODE_WRITE = (1 << 1)
		};

		CFileBase();
		virtual	~CFileBase();

		void			Destroy();
		void			Close();
		
		bool			Create(const char* filename, EFileMode mode);
		std::uint32_t Size();
		void			SeekCur(std::uint32_t size);
		void			Seek(std::uint32_t offset);
		std::uint32_t GetPosition();

		virtual bool	Write(const void* src, int bytes);
		bool			Read(void* dest, int bytes);

		char*			GetFileName();
		bool			IsNull();
		
	protected:
		int				m_mode;
		char			m_filename[261];
		Platform::Filesystem::File m_file;
		std::uint32_t m_dwSize;
};

#endif
