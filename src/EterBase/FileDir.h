#ifndef __ETER_FILE_DIR__
#define __ETER_FILE_DIR__

class CDir
{
	public:
		CDir();
		virtual ~CDir();
		
		void Destroy();		
		bool Create(const char* c_szFilter, const char* c_szPath="", bool bCheckedExtension = false);
		
	protected:
		virtual bool OnFolder(const char* c_szFilter, const char* c_szPath, const char* c_szName) = 0;
		virtual bool OnFile(const char* c_szPath, const char* c_szName) = 0;
		
	protected:
		bool IsFolder();
		
		void Initialize();
		
	protected:
		bool m_isFolder;
};


#endif
