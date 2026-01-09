/*=================================================================
 * dg_read.cpp
 *
 * Read a dgz file into MATLAB and create a MATLAB structure with
 * fields corresponding to individual elements.
 *
 * Updated for modern C++ MEX API (R2018a+)
 *
 * Usage: data = dg_read('filename.dg')
 *        data = dg_read('filename.dgz')
 *        data = dg_read('filename.lz4')
 *
 *=================================================================*/

#include "mex.hpp"
#include "mexAdapter.hpp"

#include <cstring>
#include <string>
#include <vector>
#include <memory>

#include <df.h>

#ifdef WINDOWS
#define ZLIB_DLL
#define _WINDOWS
#else
#include <unistd.h>
#endif

#include <zlib.h>
#include "dynio.h"

using namespace matlab::data;
using matlab::mex::ArgumentList;

/*
 * Helper: Uncompress gzipped input to output file
 */
static void gz_uncompress(gzFile in, FILE *out)
{
    char buf[2048];
    int len;

    for (;;) {
        len = gzread(in, buf, sizeof(buf));
        if (len < 0) return;
        if (len == 0) break;

        if (static_cast<int>(fwrite(buf, 1, static_cast<unsigned>(len), out)) != len) {
            return;
        }
    }
}

/*
 * Helper: Uncompress a gzipped file to a temp file, return FILE* to temp
 */
static FILE *uncompress_file(const char *filename, char tempname[256])
{
    FILE *fp;
    int fd;
    gzFile in;

#ifdef WINDOWS
    const char *tmptemplate = "c:/windows/temp/dgzXXXXXX";
#else
    const char *tmptemplate = "/tmp/dgzXXXXXX";
#endif

    if (!filename) return nullptr;

    if (!(in = gzopen(filename, "rb"))) {
        return nullptr;
    }

    strncpy(tempname, tmptemplate, 255);
    tempname[255] = '\0';

    fd = mkstemp(tempname);
    if (fd < 1) {
        gzclose(in);
        return nullptr;
    }
    
    fp = fdopen(fd, "wb");
    if (!fp) {
        close(fd);
        gzclose(in);
        return nullptr;
    }
    
    gz_uncompress(in, fp);
    gzclose(in);
    fclose(fp);

    fp = fopen(tempname, "rb");
    return fp;
}

/*
 * Main MEX Function Class
 */
class MexFunction : public matlab::mex::Function {
private:
    std::shared_ptr<matlab::engine::MATLABEngine> matlabPtr;
    ArrayFactory factory;

    /*
     * Throw a MATLAB error
     */
    void throwError(const std::string& msg) {
        matlabPtr->feval(u"error", 0,
            std::vector<Array>({ factory.createScalar(msg) }));
    }

    /*
     * Print to MATLAB console (like mexPrintf)
     */
    void print(const std::string& msg) {
        matlabPtr->feval(u"fprintf", 0,
            std::vector<Array>({ factory.createScalar(msg) }));
    }

    /*
     * Convert a DYN_LIST to a MATLAB Array
     * Handles nested lists (cell arrays), numeric types, and strings
     */
    Array dynListToArray(DYN_LIST *dl) {
        if (!dl) {
            return factory.createArray<double>({0, 0});
        }

        int n = DYN_LIST_N(dl);
        
        switch (DYN_LIST_DATATYPE(dl)) {
        case DF_LIST:
            {
                // Create cell array for nested lists
                DYN_LIST **sublists = reinterpret_cast<DYN_LIST **>(DYN_LIST_VALS(dl));
                std::vector<Array> cells;
                cells.reserve(n);
                
                for (int i = 0; i < n; i++) {
                    cells.push_back(dynListToArray(sublists[i]));
                }
                
                // Create n x 1 cell array
                CellArray cellArray = factory.createCellArray({static_cast<size_t>(n), 1});
                for (int i = 0; i < n; i++) {
                    cellArray[i] = cells[i];
                }
                return cellArray;
            }

        case DF_LONG:
            {
                int *vals = reinterpret_cast<int *>(DYN_LIST_VALS(dl));
                TypedArray<double> arr = factory.createArray<double>({static_cast<size_t>(n), 1});
                for (int i = 0; i < n; i++) {
                    arr[i] = static_cast<double>(vals[i]);
                }
                return arr;
            }

        case DF_SHORT:
            {
                short *vals = reinterpret_cast<short *>(DYN_LIST_VALS(dl));
                TypedArray<double> arr = factory.createArray<double>({static_cast<size_t>(n), 1});
                for (int i = 0; i < n; i++) {
                    arr[i] = static_cast<double>(vals[i]);
                }
                return arr;
            }

        case DF_FLOAT:
            {
                float *vals = reinterpret_cast<float *>(DYN_LIST_VALS(dl));
                TypedArray<double> arr = factory.createArray<double>({static_cast<size_t>(n), 1});
                for (int i = 0; i < n; i++) {
                    arr[i] = static_cast<double>(vals[i]);
                }
                return arr;
            }

        case DF_CHAR:
            {
                char *vals = reinterpret_cast<char *>(DYN_LIST_VALS(dl));
                TypedArray<double> arr = factory.createArray<double>({static_cast<size_t>(n), 1});
                for (int i = 0; i < n; i++) {
                    arr[i] = static_cast<double>(vals[i]);
                }
                return arr;
            }

        case DF_DOUBLE:
            {
                double *vals = reinterpret_cast<double *>(DYN_LIST_VALS(dl));
                TypedArray<double> arr = factory.createArray<double>({static_cast<size_t>(n), 1});
                for (int i = 0; i < n; i++) {
                    arr[i] = vals[i];
                }
                return arr;
            }

        case DF_STRING:
            {
                const char **vals = reinterpret_cast<const char **>(DYN_LIST_VALS(dl));
                // Create cell array of strings
                CellArray cellArray = factory.createCellArray({static_cast<size_t>(n), 1});
                for (int i = 0; i < n; i++) {
                    if (vals[i]) {
                        cellArray[i] = factory.createCharArray(vals[i]);
                    } else {
                        cellArray[i] = factory.createCharArray("");
                    }
                }
                return cellArray;
            }

        default:
            // Return empty array for unknown types
            return factory.createArray<double>({0, 0});
        }
    }

    /*
     * Check if filename has a specific extension (case-insensitive for lz4)
     */
    bool hasExtension(const std::string& filename, const std::string& ext) {
        size_t dotPos = filename.rfind('.');
        if (dotPos == std::string::npos) return false;
        
        std::string fileExt = filename.substr(dotPos);
        if (ext == ".lz4") {
            return (fileExt == ".lz4" || fileExt == ".LZ4");
        }
        return (fileExt == ext);
    }

public:
    MexFunction() {
        matlabPtr = getEngine();
    }

    void operator()(ArgumentList outputs, ArgumentList inputs) {
        // Validate arguments
        if (inputs.size() != 1) {
            throwError("usage: dg_read('filename')");
        }
        if (outputs.size() > 1) {
            throwError("Too many output arguments.");
        }
        if (inputs[0].getType() != ArrayType::CHAR) {
            throwError("Filename must be a string.");
        }

        // Get filename
        CharArray filenameArray = inputs[0];
        std::string filename = filenameArray.toAscii();
        
        DYN_GROUP *dg = nullptr;
        FILE *fp = nullptr;
        char tempname[256] = {0};
        bool needCleanup = false;
        bool dgLoaded = false;

        // Determine file type and load accordingly
        if (hasExtension(filename, ".dg") && !hasExtension(filename, ".dgz")) {
            // Plain .dg file - no decompression needed
            fp = fopen(filename.c_str(), "rb");
            if (!fp) {
                throwError("Error opening data file \"" + filename + "\".");
            }
        }
        else if (hasExtension(filename, ".lz4")) {
            // LZ4 compressed file - use direct reader
            dg = dfuCreateDynGroup(4);
            if (!dg) {
                throwError("dg_read: error creating new dyngroup");
            }
            if (dgReadDynGroup(const_cast<char*>(filename.c_str()), dg) == DF_OK) {
                dgLoaded = true;
            } else {
                dfuFreeDynGroup(dg);
                throwError("dg_read: file " + filename + " not recognized as lz4/dg format");
            }
        }
        else {
            // Try to uncompress (dgz or gz file)
            fp = uncompress_file(filename.c_str(), tempname);
            if (!fp) {
                // Try with .dg extension
                std::string tryname = filename + ".dg";
                fp = uncompress_file(tryname.c_str(), tempname);
            }
            if (!fp) {
                // Try with .dgz extension
                std::string tryname = filename + ".dgz";
                fp = uncompress_file(tryname.c_str(), tempname);
            }
            if (!fp) {
                throwError("dg_read: file " + filename + " not found");
            }
            needCleanup = (tempname[0] != 0);
        }

        // If we have a file pointer, read the dg structure from it
        if (!dgLoaded && fp) {
            dg = dfuCreateDynGroup(4);
            if (!dg) {
                fclose(fp);
                if (needCleanup) unlink(tempname);
                throwError("Error creating dyn group.");
            }

            if (!dguFileToStruct(fp, dg)) {
                dfuFreeDynGroup(dg);
                fclose(fp);
                if (needCleanup) unlink(tempname);
                throwError("dg_read: file " + filename + " not recognized as dg format");
            }
            fclose(fp);
            if (needCleanup) unlink(tempname);
        }

        // Convert DYN_GROUP to MATLAB struct
        int nLists = DYN_GROUP_NLISTS(dg);
        
        // Collect field names
        std::vector<std::string> fieldNames;
        fieldNames.reserve(nLists);
        for (int i = 0; i < nLists; i++) {
            const char* name = DYN_LIST_NAME(DYN_GROUP_LIST(dg, i));
            fieldNames.push_back(name ? name : "");
        }

        // Create struct with field names
        StructArray result = factory.createStructArray({1, 1}, fieldNames);

        // Fill in field values
        for (int i = 0; i < nLists; i++) {
            Array fieldValue = dynListToArray(DYN_GROUP_LIST(dg, i));
            result[0][fieldNames[i]] = fieldValue;
        }

        dfuFreeDynGroup(dg);
        
        outputs[0] = result;
    }
};
