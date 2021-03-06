/**
 *   j0llyDmpr
 *   Copyright (C) 2010 Souchet Axel <0vercl0k@tuxfamily.org>
 *
 *   This file is part of j0llyDmpr.
 *
 *   J0llyDmpr is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   J0llyDmpr is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with J0llyDmpr. If not, see <http://www.gnu.org/licenses/>.
**/
#include "usb.h"
#include "debug.h"

/** Definitions **/

UCHAR GetLetterOfNewVolume(const DWORD precVal, const DWORD newVal)
{
	#define GetBit(x, nb) ((x >> nb) & 0x1)
	UCHAR i = 0;

	TRACEMSG();

	for(; i < (sizeof(DWORD)*8); ++i)
	{
		if(GetBit(precVal, i) != GetBit(newVal, i))
			return 'A' + i;
	}

	return 0;
}

BOOL DumpAndSearchInteresstingFiles(const PUCHAR pVol, const DWORD lvl, PCONFIG pConf)
{
    WIN32_FIND_DATA findData = {0};
    DWORD64 fileSize = 0;
	PUCHAR researchPatternPath = NULL,
        directoryPath = NULL,
        filePathToCopy = NULL,
        filePathCopied = NULL;
    HANDLE hFind = 0;
    SIZE_T sizeStr = strlen(pVol) + 1 + 1,
		sizeStrDirectory = 0,
		sizeFilePathToCopy = 0,
        sizeFilePathCopied = 0;
	DWORD status = 0;
    BOOL ret = TRUE;

    TRACEMSG();

    researchPatternPath = (PUCHAR)malloc(sizeof(UCHAR) * sizeStr);
    if(researchPatternPath == NULL)
        return FALSE;

    ZeroMemory(researchPatternPath, sizeStr);
    strncpy(researchPatternPath, pVol, strlen(pVol));
    strncat(researchPatternPath, "*", 1);

    hFind = FindFirstFile(
        researchPatternPath,
        &findData
    );

    if(hFind == INVALID_HANDLE_VALUE)
    {
        free(researchPatternPath);
        return FALSE;
    }

    do
    {
        if(strchr(findData.cFileName, '\\') != NULL || strchr(findData.cFileName, '/') != NULL)
        {
            DEBUGMSG("The file '%s' is not correct for security reasons.");
            continue;
        }

        if((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
        {
			fileSize = ((DWORD64)findData.nFileSizeHigh << (sizeof(DWORD) * 8)) | findData.nFileSizeLow;
            if(isAnInteresstingFile(findData.cFileName, fileSize, pConf) == 1)
			{
				sizeFilePathToCopy = strlen(pVol) + strlen(findData.cFileName) + 1;
                sizeFilePathCopied = strlen(pConf->outputPath) + strlen(findData.cFileName) + 1;

                filePathToCopy = (PUCHAR)malloc(sizeof(char) * sizeFilePathToCopy);
                filePathCopied = (PUCHAR)malloc(sizeof(char) * sizeFilePathCopied);

                if(filePathToCopy == NULL || filePathCopied == NULL)
                {
                    ret = FALSE;
                    goto clean;
                }

                ZeroMemory(filePathToCopy, sizeFilePathToCopy);
                ZeroMemory(filePathCopied, sizeFilePathCopied);

                strncpy(filePathToCopy, pVol, strlen(pVol));
                strncat(filePathToCopy, findData.cFileName, strlen(findData.cFileName));

                strncpy(filePathCopied, pConf->outputPath, strlen(pConf->outputPath));
                strncat(filePathCopied, findData.cFileName, strlen(findData.cFileName));

                DEBUGMSG("Copying '%s' to '%s'..", filePathToCopy, filePathCopied);

                status = CopyFile(
                    filePathToCopy,
                    filePathCopied,
                    TRUE
                );

                if(status != 0)
                {
                    status = GetFileAttributes(
                        filePathCopied
                    );

                    if(status != INVALID_FILE_ATTRIBUTES && (status & FILE_ATTRIBUTE_HIDDEN) != 0)
                    {
                        SetFileAttributes(
                            filePathCopied,
                            status & (~FILE_ATTRIBUTE_HIDDEN)
                        );
                    }
                }

                free(filePathToCopy);
                free(filePathCopied);

                filePathToCopy = filePathCopied = NULL;
             }
        }
        else
        {
            if(lvl != (pConf->recurse_max - 1) && strcmp(findData.cFileName, ".") != 0 && strcmp(findData.cFileName, "..") != 0)
            {
                sizeStrDirectory = strlen(pVol) + strlen(findData.cFileName) + 1 + 1;
                directoryPath = (PUCHAR)malloc(sizeof(char) * sizeStrDirectory);
                if(directoryPath == NULL)
                {
                    ret = FALSE;
                    goto clean;
                }

                ZeroMemory(directoryPath, sizeStrDirectory);
                strncpy(directoryPath, pVol, strlen(pVol));
                strncat(directoryPath, findData.cFileName, strlen(findData.cFileName));
                strncat(directoryPath, "\\", 1);

                DumpAndSearchInteresstingFiles(directoryPath, lvl+1, pConf);
                free(directoryPath);
            }
        }

    }while(FindNextFile(hFind, &findData) != 0);

    clean:

    if(filePathToCopy != NULL)
        free(filePathToCopy);

    if(filePathCopied != NULL)
        free(filePathCopied);

    free(researchPatternPath);
    FindClose(hFind);

    return ret;
}

BOOL isAnInteresstingFile(const PUCHAR file, const unsigned long long fileSize, PCONFIG pConf)
{
    PUCHAR str = NULL;
	SIZE_T sizeStr = strlen(file) + 1;
    DWORD i = 0;
    BOOL ret = FALSE;

    TRACEMSG();

    str = (PUCHAR)malloc(sizeof(char) * sizeStr);
    if(str == NULL)
        return FALSE;

    ZeroMemory(str, sizeStr);

    for(; i < sizeStr - 1; ++i)
        str[i] = tolower(file[i]);

    for(i = 0; i < pConf->nbPattern; ++i)
    {
        if(strstr(str, pConf->patterns[i]) != NULL && fileSize <= pConf->max_size)
        {
            ret = TRUE;
            goto clean;
        }
    }

    clean:

    if(str != NULL)
        free(str);

    return ret;
}

VOID initUsbStuff(const char* outpath)
{
    TRACEMSG();

    CreateDirectory(outpath,
        NULL
    );
}
