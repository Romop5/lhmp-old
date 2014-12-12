#include "CFileTransfer.h"

#include "CCore.h"


extern CCore* g_CCore;

CFile::CFile(unsigned int _ID, FILE* _handle, char _name[], char _checksum[], int _size)
{
	this->ID = _ID;
	this->handle = _handle;
	sprintf(this->name, "%s", _name);
	sprintf(this->checksum, "%s", _checksum);
	this->size = _size;
	this->alreadyWritten = 0;

	rewind(_handle);
}
CFile::~CFile()
{
	if (handle != NULL)
	{
		fclose(handle);
	}
}
char*	CFile::GetName()
{
	return this->name;
}
char*	CFile::GetCheckSum()
{
	return this->checksum;
}
int		CFile::GetSize()
{
	return this->size;
}
unsigned int	CFile::GetID()
{
	return this->ID;
}
FILE*			CFile::GetFileHandle()
{
	return this->handle;
}
void			CFile::WriteBytes(unsigned char data[], int len)
{
	if (this->handle != NULL)
	{
		fwrite(data, 1, len, this->handle);
	}
	alreadyWritten += len;
	
	char buff[250];
	sprintf(buff, "Data arrived: %d/%d", alreadyWritten, this->GetSize());
	g_CCore->GetChat()->AddMessage(buff);
}

void CFile::CloseHandle()
{
	if (handle != NULL)
	{
		fclose(handle);
		handle = NULL;
	}
}


int	CFile::GetAlreadyWritten()
{
	return (unsigned int) this->alreadyWritten;
}

//***************************************************************************

void CFileTransfer::InitTransfer(RakNet::BitStream* stream)
{
	CreateDirectory("lhmp/files", NULL);
	this->status = FILETRANSFER_STATE::CHECKING_INTEGRITY;

	int count;
	stream->Read(count);

	// file count
	int ID;
	char checksum[65];
	char name[256];
	unsigned int size;

	for (int i = 0; i < count; i++)
	{
		stream->Read(ID);
		stream->Read(checksum);
		stream->Read(name);
		stream->Read(size);

		char path[512];
		sprintf(path, "lhmp/files/%s", checksum);
		FILE* file = fopen(path, "rb");
		if (file != NULL)
		{
			char fileChecksum[65];
			MD5File(file, fileChecksum);

			if (strcmp(fileChecksum, checksum) == 0)
			{
				// Add to our file system
				g_CCore->GetFileSystem()->AddFile(name, checksum);
				continue;
			}
			else {
				fclose(file);
				fclose(fopen(path, "w"));
			}
		}

		file = fopen(path, "wb");
		if (file != NULL)
		{
			CFile* fuckgod = new CFile(ID, file, name, checksum, (int) size);
			this->fileList.push_back(fuckgod);
		}
		else
		{
			g_CCore->GetChat()->AddMessage("Open file failed, what now ? Fuck !");
		}
	}

	RakNet::BitStream out;
	out.Write((RakNet::MessageID) ID_FILETRANSFER);
	out.Write((RakNet::MessageID) FILETRANSFER_INIT);
	out.Write((int)this->fileList.size());

	for (unsigned int i = 0; i < this->fileList.size(); i++)
	{
		out.Write(this->fileList[i]->GetID());
	}
	g_CCore->GetNetwork()->GetPeer()->Send(&out, HIGH_PRIORITY, RELIABLE_ORDERED, 0, UNASSIGNED_RAKNET_GUID, true);
	if (this->fileList.size() > 0)
		this->status = FILETRANSFER_STATE::DOWNLOADING;
};

CFileTransfer::CFileTransfer()
{
	this->status = FILETRANSFER_STATE::NO_TRANSFER;
	this->fileList.clear();
	this->downloadingID = 0;
}

void CFileTransfer::HandlePacket(RakNet::BitStream* stream)
{
	RakNet::MessageID type;
	stream->Read(type);
	switch (type)
	{
	case FILETRANSFER_INIT:
	{
							  // file transfer init
							  this->InitTransfer(stream);
	}
		break;
	case FILETRANSFER_SEND:
	{
							  // file transfer - receiving file
							  this->status = FILETRANSFER_STATE::DOWNLOADING;
								unsigned char data[65537];
								int ID, bytesCount;
								stream->Read(ID);
								stream->Read(bytesCount);

								for (int i = 0; i < bytesCount; i++)
									stream->Read(data[i]);

								for (unsigned int i = 0; i < this->fileList.size(); i++)
								{
									if (this->fileList[i]->GetID() == ID)
									{
										this->fileList[i]->WriteBytes(data, bytesCount);
										this->downloadingID = ID;
										break;
									}
								}

								RakNet::BitStream out;
								out.Write((RakNet::MessageID) ID_FILETRANSFER);
								out.Write((RakNet::MessageID) FILETRANSFER_SEND);
								g_CCore->GetNetwork()->GetPeer()->Send(&out, HIGH_PRIORITY, RELIABLE_ORDERED, 0, UNASSIGNED_RAKNET_GUID, true);

	}
		break;
	case FILETRANSFER_FINISH:
	{
								// file transfer has finished
								this->status = FILETRANSFER_STATE::NO_TRANSFER;
								// confirm it
								RakNet::BitStream out;
								out.Write((RakNet::MessageID) ID_FILETRANSFER);
								out.Write((RakNet::MessageID) FILETRANSFER_FINISH);
								g_CCore->GetNetwork()->GetPeer()->Send(&out, HIGH_PRIORITY, RELIABLE_ORDERED, 0, UNASSIGNED_RAKNET_GUID, true);

								// handle it
								for (unsigned int i = 0; i < this->fileList.size(); i++)
								{
									// Add to our file system
									g_CCore->GetFileSystem()->AddFile(this->fileList[i]->GetName(), this->fileList[i]->GetCheckSum());
									this->fileList[i]->CloseHandle();
								}
	}
		break;
	default:
		break;
	}
}


void CFileTransfer::Render()
{
	if (this->status != FILETRANSFER_STATE::NO_TRANSFER)
	{
		Vector2D screen = g_CCore->GetGraphics()->GetResolution();
		if (this->status == FILETRANSFER_STATE::CHECKING_INTEGRITY)
		{
			g_CCore->GetGraphics()->GetFont()->DrawTextA("#CHECKING FILE INTEGRITY", screen.x / 2, screen.y / 2,0xFFFF0000);
		}
		else {
			for (unsigned int i = 0; i < this->fileList.size(); i++)
			{
				if (downloadingID == this->fileList[i]->GetID())
				{
					char buff[255];
					sprintf(buff, "DOWNLOADING FILE '%s'", this->fileList[i]->GetName());
					g_CCore->GetGraphics()->GetFont()->DrawTextA(buff, screen.x / 2, screen.y / 2, 0xFFFF0000);

					float ratio = this->fileList[i]->GetAlreadyWritten() / (float)this->fileList[i]->GetSize();
					g_CCore->GetGraphics()->FillARGB((screen.x / 2), (screen.y / 2) + 50, 200, 10, 0xaa000000);
					g_CCore->GetGraphics()->FillARGB((screen.x / 2), (screen.y / 2) + 50, (int)(200 * ratio), 10, 0xFFff0000);

					sprintf(buff, "%.2f%sB/%.2f%sB", Tools::GetMetricUnitNum((float)this->fileList[i]->GetAlreadyWritten()), Tools::MetricUnits[Tools::GetMetricUnitIndex((float)this->fileList[i]->GetAlreadyWritten())],
						Tools::GetMetricUnitNum((float)this->fileList[i]->GetSize()), Tools::MetricUnits[Tools::GetMetricUnitIndex((float)this->fileList[i]->GetSize())]);
					g_CCore->GetGraphics()->GetFont()->DrawTextA(buff, screen.x / 2, (screen.y / 2) + 65, 0xFFFF0000);
				}
			}
		}
	}
}