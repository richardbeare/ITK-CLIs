////program for binning images by averaging
//01: based on template_02.cxx
//02: based on template_vec.cxx
// SCIFIO:  https://github.com/scifio/scifio-imageio/blob/master/test/itkSCIFIOImageIOTest.cxx


#include <complex>

#include "itkFilterWatcher.h"
#include <itkImageFileReader.h>
#include <itkBinShrinkImageFilter.h>
#include <itkImageFileWriter.h>
#include <itkSCIFIOImageIO.h>

#ifdef USE_SDI
#include <itkPipelineMonitorImageFilter.h>
#endif



template<typename InputComponentType, typename InputPixelType, size_t CompPerPixel, size_t Dimension>
int DoIt(int argc, char *argv[]){

    typedef InputPixelType  OutputPixelType;

    typedef itk::Image<InputPixelType, Dimension>  InputImageType;
    typedef itk::Image<OutputPixelType, Dimension>  OutputImageType;

    itk::SCIFIOImageIO::Pointer io= itk::SCIFIOImageIO::New();
    io->DebugOn();

    typedef itk::ImageFileReader<InputImageType> ReaderType;
    typename ReaderType::Pointer reader = ReaderType::New();

    reader->SetFileName(argv[1]);
    reader->ReleaseDataFlagOn();
    reader->SetImageIO(io);
#ifndef USE_SDI
    FilterWatcher watcherI(reader);
    watcherI.QuietOn();
    watcherI.ReportTimeOn();
    try{
        reader->Update();
        }
    catch(itk::ExceptionObject &ex){
        std::cerr << ex << std::endl;
        return EXIT_FAILURE;
        }
#endif

    const typename InputImageType::Pointer& input= reader->GetOutput();



    typedef itk::BinShrinkImageFilter<InputImageType, OutputImageType> FilterType;
    typename FilterType::Pointer filter= FilterType::New();
    filter->SetInput(input);
    filter->SetShrinkFactors(atoi(argv[4]));
    filter->ReleaseDataFlagOn();

#ifndef USE_SDI
    FilterWatcher watcher1(filter);
    try{
        filter->Update();
        }
    catch(itk::ExceptionObject &ex){
        std::cerr << ex << std::endl;
        return EXIT_FAILURE;
        }
#else
    typedef itk::PipelineMonitorImageFilter<OutputImageType> MonitorFilterType;
    typename MonitorFilterType::Pointer monitorFilter = MonitorFilterType::New();
    monitorFilter->SetInput(filter->GetOutput());
#endif

    itk::SCIFIOImageIO::Pointer ioOut= itk::SCIFIOImageIO::New();
    ioOut->DebugOn();

    typedef itk::ImageFileWriter<OutputImageType>  WriterType;
    typename WriterType::Pointer writer = WriterType::New();

    FilterWatcher watcherO(writer);
    writer->SetFileName(argv[2]);
    writer->SetImageIO(ioOut);
#ifndef USE_SDI
    writer->SetInput(filter->GetOutput());
    writer->SetUseCompression(atoi(argv[3]));
#else
    writer->SetInput(monitorFilter->GetOutput());
    writer->UseCompressionOff(); //writer->SetUseCompression(atoi(argv[3]));//writing compressed is not supported for streaming!
    writer->SetNumberOfStreamDivisions(atoi(argv[3]));
#endif
    try{
        writer->Update();
        }
    catch(itk::ExceptionObject &ex){
        std::cerr << ex << std::endl;
        return EXIT_FAILURE;
        }

#ifdef USE_SDI
    if (!monitorFilter->VerifyAllInputCanStream(atoi(argv[3]))){ // reports a warning if expected and actual # chunks differ
        // std::cerr << monitorFilter;
        }
#endif

    return EXIT_SUCCESS;

    }


template<typename InputComponentType, size_t CompPerPixel, size_t Dimension>
int dispatch_pT(itk::ImageIOBase::IOPixelType pixelType, int argc, char *argv[]){
    int res= EXIT_FAILURE;
    //http://www.itk.org/Doxygen45/html/classitk_1_1ImageIOBase.html#abd189f096c2a1b3ea559bc3e4849f658
    //http://www.itk.org/Doxygen45/html/itkImageIOBase_8h_source.html#l00099
    //IOPixelType:: UNKNOWNPIXELTYPE, SCALAR, RGB, RGBA, OFFSET, VECTOR, POINT, COVARIANTVECTOR, SYMMETRICSECONDRANKTENSOR, DIFFUSIONTENSOR3D, COMPLEX, FIXEDARRAY, MATRIX

    switch (pixelType){
    case itk::ImageIOBase::SCALAR:{
        typedef InputComponentType InputPixelType;
        res= DoIt<InputComponentType, InputPixelType, CompPerPixel, Dimension>(argc, argv);
        } break;
    case itk::ImageIOBase::RGB:{
        typedef itk::RGBPixel<InputComponentType> InputPixelType;
        res= DoIt<InputComponentType, InputPixelType, CompPerPixel, Dimension>(argc, argv);
        } break;
    case itk::ImageIOBase::RGBA:{
        typedef itk::RGBAPixel<InputComponentType> InputPixelType;
        res= DoIt<InputComponentType, InputPixelType, CompPerPixel, Dimension>(argc, argv);
        } break;
    case itk::ImageIOBase::COMPLEX:{
        typedef std::complex<InputComponentType> InputPixelType;
        res= DoIt<InputComponentType, InputPixelType, CompPerPixel, Dimension>(argc, argv);
        } break;
    case itk::ImageIOBase::VECTOR:{
        typedef itk::Vector<InputComponentType, CompPerPixel> InputPixelType;
        res= DoIt<InputComponentType, InputPixelType, CompPerPixel, Dimension>(argc, argv);
        } break;
    case itk::ImageIOBase::UNKNOWNPIXELTYPE:
    default:
        std::cerr << std::endl << "Error: Pixel type not handled!" << std::endl;
        break;
        }//switch
    return res;
    }

template<typename InputComponentType, size_t CompPerPixel>
int dispatch_D(itk::ImageIOBase::IOPixelType pixelType, size_t dimensionType, int argc, char *argv[]){
    int res= EXIT_FAILURE;
    switch (dimensionType){
    case 1:
        res= dispatch_pT<InputComponentType, CompPerPixel, 1>(pixelType, argc, argv);
        break;
    case 2:
        res= dispatch_pT<InputComponentType, CompPerPixel, 2>(pixelType, argc, argv);
        break;
    case 3:
        res= dispatch_pT<InputComponentType, CompPerPixel, 3>(pixelType, argc, argv);
        break;
    default:
        std::cerr << "Error: Images of dimension " << dimensionType << " are not handled!" << std::endl;
        break;
        }//switch
    return res;
    }

template<typename InputComponentType>
int dispatch_cPP(size_t compPerPixel, itk::ImageIOBase::IOPixelType pixelType, size_t dimensionType, int argc, char *argv[]){
    int res= EXIT_FAILURE;
    switch (compPerPixel){
    case 1:
        res= dispatch_D<InputComponentType, 1>(pixelType, dimensionType, argc, argv);
        break;
    case 2:
        res= dispatch_D<InputComponentType, 2>(pixelType, dimensionType, argc, argv);
        break;
    case 3:
        res= dispatch_D<InputComponentType, 3>(pixelType, dimensionType, argc, argv);
        break;
    case 4:
        res= dispatch_D<InputComponentType, 4>(pixelType, dimensionType, argc, argv);
        break;
    // case 5:
    //     res= dispatch_D<InputComponentType, 5>(pixelType, dimensionType, argc, argv);
    //     break;
    default:
        std::cerr << "Error: NumberOfComponentsPerPixel (" << compPerPixel << ") not handled!" << std::endl;
        break;
        }//switch
    return res;
    }

int dispatch_cT(itk::ImageIOBase::IOComponentType componentType, size_t compPerPixel, itk::ImageIOBase::IOPixelType pixelType, size_t dimensionType, int argc, char *argv[]){
    int res= EXIT_FAILURE;

    //http://www.itk.org/Doxygen45/html/classitk_1_1ImageIOBase.html#a8dc783055a0af6f0a5a26cb080feb178
    //http://www.itk.org/Doxygen45/html/itkImageIOBase_8h_source.html#l00107
    //IOComponentType: UNKNOWNCOMPONENTTYPE, UCHAR, CHAR, USHORT, SHORT, UINT, INT, ULONG, LONG, FLOAT, DOUBLE

    switch (componentType){
    case itk::ImageIOBase::UCHAR:{        // uint8_t
        typedef unsigned char InputComponentType;
        res= dispatch_cPP<InputComponentType>(compPerPixel, pixelType, dimensionType, argc, argv);
        } break;
    case itk::ImageIOBase::CHAR:{         // int8_t
        typedef char InputComponentType;
        res= dispatch_cPP<InputComponentType>(compPerPixel, pixelType, dimensionType, argc, argv);
        } break;
    case itk::ImageIOBase::USHORT:{       // uint16_t
        typedef unsigned short InputComponentType;
        res= dispatch_cPP<InputComponentType>(compPerPixel, pixelType, dimensionType, argc, argv);
        } break;
    case itk::ImageIOBase::SHORT:{        // int16_t
        typedef short InputComponentType;
        res= dispatch_cPP<InputComponentType>(compPerPixel, pixelType, dimensionType, argc, argv);
        } break;
    case itk::ImageIOBase::UINT:{         // uint32_t
        typedef unsigned int InputComponentType;
        res= dispatch_cPP<InputComponentType>(compPerPixel, pixelType, dimensionType, argc, argv);
        } break;
    case itk::ImageIOBase::INT:{          // int32_t
        typedef int InputComponentType;
        res= dispatch_cPP<InputComponentType>(compPerPixel, pixelType, dimensionType, argc, argv);
        } break;
    case itk::ImageIOBase::ULONG:{        // uint64_t
        typedef unsigned long InputComponentType;
        res= dispatch_cPP<InputComponentType>(compPerPixel, pixelType, dimensionType, argc, argv);
        } break;
    case itk::ImageIOBase::LONG:{         // int64_t
        typedef long InputComponentType;
        res= dispatch_cPP<InputComponentType>(compPerPixel, pixelType, dimensionType, argc, argv);
        } break;
    case itk::ImageIOBase::FLOAT:{        // float32
        typedef float InputComponentType;
        res= dispatch_cPP<InputComponentType>(compPerPixel, pixelType, dimensionType, argc, argv);
        } break;
    case itk::ImageIOBase::DOUBLE:{       // float64
        typedef double InputComponentType;
        res= dispatch_cPP<InputComponentType>(compPerPixel, pixelType, dimensionType, argc, argv);
        } break;
    case itk::ImageIOBase::UNKNOWNCOMPONENTTYPE:
    default:
        std::cerr << "unknown component type" << std::endl;
        break;
        }//switch
    return res;
    }


////from http://itk-users.7.n7.nabble.com/Pad-image-with-0-but-keep-its-type-what-ever-it-is-td27442.html
//namespace itk{
  // Description:
  // Get the PixelType and ComponentType from fileName

void GetImageType (std::string fileName,
    itk::ImageIOBase::IOPixelType &pixelType,
    itk::ImageIOBase::IOComponentType &componentType,
    size_t &compPerPixel,
    size_t &dimensionType
    ){
    typedef itk::VectorImage<char, 1> ImageType; //template initialization parameters need to be given but can be arbitrary here
    itk::ImageFileReader<ImageType>::Pointer imageReader= itk::ImageFileReader<ImageType>::New();
    imageReader->SetFileName(fileName.c_str());
    imageReader->UpdateOutputInformation();

    pixelType = imageReader->GetImageIO()->GetPixelType();
    componentType = imageReader->GetImageIO()->GetComponentType();
    dimensionType= imageReader->GetImageIO()->GetNumberOfDimensions();
    compPerPixel = imageReader->GetOutput()->GetNumberOfComponentsPerPixel(); // needs VectorImage

    std::cerr << std::endl << "dimensions: " << dimensionType << std::endl;
    std::cerr << "component type: " << imageReader->GetImageIO()->GetComponentTypeAsString(componentType) << std::endl;
    std::cerr << "component size: " << imageReader->GetImageIO()->GetComponentSize() << std::endl;
    std::cerr << "pixel type (string): " << imageReader->GetImageIO()->GetPixelTypeAsString(imageReader->GetImageIO()->GetPixelType()) << std::endl;
    std::cerr << "pixel type: " << pixelType << std::endl << std::endl;
    std::cerr << "NumberOfComponentsPerPixel: " << compPerPixel << std::endl;

    }



int main(int argc, char *argv[]){
    if ( argc != 5 ){
        std::cerr << "Missing Parameters: "
                  << argv[0]
                  << " Input_Image"
                  << " Output_Image"
#ifndef USE_SDI
                  << " compress"
#else
                  << " stream-chunks"
#endif
                  << " bin"
                  << std::endl;

        return EXIT_FAILURE;
        }

    if(atoi(argv[3]) < 0){
        std::cerr << "3rd parameter must not be negative" << std::endl;
        return EXIT_FAILURE;
        }

#ifndef USE_SDI
    if(atoi(argv[3]) > 1){
        std::cerr << "compress must be 0 or 1 (to avoid confusion with stream-chunks of SDI version" << std::endl;
        return EXIT_FAILURE;
        }
#else
    if(atoi(argv[3]) < 2){
        std::cerr << "stream-chunks must be > 1 (or use non-SDI version)" << std::endl;
        return EXIT_FAILURE;
        }
#endif


    itk::ImageIOBase::IOPixelType pixelType;
    typename itk::ImageIOBase::IOComponentType componentType;
    size_t compPerPixel;
    size_t dimensionType;


    try {
        GetImageType(argv[1], pixelType, componentType, compPerPixel, dimensionType);
        }//try
    catch( itk::ExceptionObject &excep){
        std::cerr << argv[0] << ": exception caught !" << std::endl;
        std::cerr << excep << std::endl;
        return EXIT_FAILURE;
        }

    return dispatch_cT(componentType, compPerPixel, pixelType, dimensionType, argc, argv);
    }






