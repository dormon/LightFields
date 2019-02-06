#include <Compressonator.h>
#include <DDS_Helpers.h>
#include <FreeImagePlus.h>
#include <iostream>
#include <vector>
#include <stdexcept>

void printHelp()
{
   std::cerr << "Usage: convertLf inputPath quality" << std::endl
             << "quality <0.0,1.0> affects the texture compression quality/speed" << std::endl
             << "Expected input directory structure: info (lf info file), 0,1,... (dirs for each camera frames, frames in dirs numbered the same way)" << std::endl;
}

void convertImage(const char *srcPath, const char *dstPath)
{
    fipImage img;
    img.load(srcPath);
    img.convertTo24Bits();
    img.flipVertical();

    CMP_Texture src;
    src.dwSize = sizeof(src);
    src.dwWidth = img.getWidth();
    src.dwHeight = img.getHeight();
    src.dwPitch = 0;
    src.format = CMP_FORMAT_BGR_888;
    src.dwDataSize = CMP_CalculateBufferSize(&src);
    src.pData = (CMP_BYTE*)FreeImage_GetBits(img);

    CMP_Texture dst;
    dst.dwSize = sizeof(dst);
    dst.dwWidth = src.dwWidth;
    dst.dwHeight = src.dwHeight;
    dst.dwPitch = 0;
    dst.format = CMP_FORMAT_BC7;
    dst.dwDataSize = CMP_CalculateBufferSize(&dst);
    std::vector<CMP_BYTE> dstData(dst.dwDataSize);
    dst.pData = dstData.data();

    CMP_CompressOptions options{0};
    options.dwSize = sizeof(options);
    options.fquality = 0.1;
    options.dwnumThreads = 8;

    std::cout << "Converting " << srcPath << std::endl;
    CMP_ERROR status = CMP_ConvertTexture(&src, &dst, &options,
                        [](float progress, CMP_DWORD_PTR user1, CMP_DWORD_PTR user2)
                        {
                            UNREFERENCED_PARAMETER(user1);
                            UNREFERENCED_PARAMETER(user2);
                            return false;
                        },
    0,0);
    //NULL, NULL);
    if(status != CMP_OK)
        throw std::runtime_error("Conversion failed.");

    SaveDDSFile(dstPath,dst);
}

int main (int argc, char** argv)
{
    if(argc<2)
    {
        printHelp();
        return 1;
    }

    try
    {
        convertImage("0.ppm", "0.dds");
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
