////program to use itk to convert between file-formats
//01: based on file-series_writer.cxx
//02: changes based on Modules/Filtering/ImageCompose/test/itkJoinSeriesImageFilterStreamingTest.cxx


#include <complex>

#include "itkFilterWatcher.h" 
#include <itkImageFileReader.h>
#include <itkNumericSeriesFileNames.h>
#include <itkExtractImageFilter.h>
#include <itkImageFileWriter.h>




int dispatch_cT(itk::ImageIOBase::IOPixelType, itk::ImageIOBase::IOComponentType, size_t, int, char **);

template<typename InputComponentType>
int dispatch_pT(itk::ImageIOBase::IOPixelType pixelType, size_t, int, char **);

template<typename InputComponentType, typename InputPixelType>
int dispatch_D(size_t, int, char **);

template<typename InputComponentType, typename InputPixelType, size_t Dimension>
int DoIt(int, char *argv[]);




// void FilterEventHandlerITK(itk::Object *caller, const itk::EventObject &event, void*){

//     const itk::ProcessObject* filter = static_cast<const itk::ProcessObject*>(caller);

//     if(itk::ProgressEvent().CheckEvent(&event))
// 	fprintf(stderr, "\r%s progress: %5.1f%%", filter->GetNameOfClass(), 100.0 * filter->GetProgress());//stderr is flushed directly
//     else if(strstr(filter->GetNameOfClass(), "ImageFileReader")){
// 	typedef itk::Image<unsigned char, 3> ImageType;
// 	const itk::ImageFileReader<ImageType>* reader = static_cast<const itk::ImageFileReader<ImageType>*>(caller);
// 	std::cerr << "Reading: " << reader->GetFileName() << std::endl;   
// 	}
//     else if(itk::EndEvent().CheckEvent(&event))
// 	std::cerr << std::endl;   
//     }



template<typename InputComponentType, typename InputPixelType, size_t Dimension>
int DoIt(int argc, char *argv[]){

    typedef InputPixelType  OutputPixelType;
    
    typedef itk::Image<InputPixelType, Dimension>  InputImageType;
    typedef itk::Image<OutputPixelType, Dimension - 1>  SliceImageType;


    typedef itk::ImageFileReader<InputImageType> ReaderType;
    typename ReaderType::Pointer reader = ReaderType::New();

    reader->SetFileName(argv[1]);
    reader->UpdateOutputInformation();

    typename InputImageType::RegionType region= reader->GetOutput()->GetLargestPossibleRegion();
    const unsigned int numberOfSlices = itk::Math::CastWithRangeCheck<unsigned int>(region.GetSize(Dimension-1));

    typedef itk::NumericSeriesFileNames    NameGeneratorType;
    NameGeneratorType::Pointer nameGenerator = NameGeneratorType::New();

    nameGenerator->SetSeriesFormat(argv[2]);
    nameGenerator->SetStartIndex(0);
    nameGenerator->SetEndIndex(numberOfSlices - 1);
    nameGenerator->SetIncrementIndex(1);
    std::vector<std::string> names = nameGenerator->GetFileNames();

    typedef itk::ExtractImageFilter<InputImageType,SliceImageType>     SliceExtractorFilterType;
    typedef itk::ImageFileWriter<SliceImageType>                       ImageFileWriterType;

    for (typename InputImageType::SizeValueType z = 0; z < numberOfSlices; ++z ){

	typename SliceExtractorFilterType::Pointer extractor = SliceExtractorFilterType::New();
	extractor->SetDirectionCollapseToSubmatrix();

	typename SliceExtractorFilterType::InputImageRegionType slice(reader->GetOutput()->GetLargestPossibleRegion());
	slice.SetSize(Dimension-1, 0);
	slice.SetIndex(Dimension-1, z);

	std::cerr << "slice region index: " << slice.GetIndex()
		  << "  size: " << slice.GetSize()
		  << std::endl;


	extractor->SetExtractionRegion(slice);
	extractor->SetInput(reader->GetOutput());
	extractor->InPlaceOn();
	extractor->ReleaseDataFlagOn();

	typename ImageFileWriterType::Pointer writer = ImageFileWriterType::New();
	FilterWatcher watcherO(writer);
	writer->SetFileName(names[z]);
	writer->SetInput(extractor->GetOutput());
	writer->SetUseCompression(atoi(argv[3]));
	try{
	    writer->Update();
	    }
	catch(itk::ExceptionObject &ex){
	    std::cerr << ex << std::endl;
	    return EXIT_FAILURE;
	    }

	}

    return EXIT_SUCCESS;
    }


int dispatch_cT(itk::ImageIOBase::IOComponentType componentType, itk::ImageIOBase::IOPixelType pixelType, size_t dimensionType, int argc, char *argv[]){
  int res= 0;

  //http://www.itk.org/Doxygen45/html/classitk_1_1ImageIOBase.html#a8dc783055a0af6f0a5a26cb080feb178
  //http://www.itk.org/Doxygen45/html/itkImageIOBase_8h_source.html#l00107
  //IOComponentType: UNKNOWNCOMPONENTTYPE, UCHAR, CHAR, USHORT, SHORT, UINT, INT, ULONG, LONG, FLOAT, DOUBLE

  switch (componentType){
  case itk::ImageIOBase::UCHAR:{        // uint8_t
    typedef unsigned char InputComponentType;
    res= dispatch_pT<InputComponentType>(pixelType, dimensionType, argc, argv);
  } break;
  case itk::ImageIOBase::CHAR:{         // int8_t
    typedef char InputComponentType;
    res= dispatch_pT<InputComponentType>(pixelType, dimensionType, argc, argv);
  } break;
  case itk::ImageIOBase::USHORT:{       // uint16_t
    typedef unsigned short InputComponentType;
    res= dispatch_pT<InputComponentType>(pixelType, dimensionType, argc, argv);
  } break;
  case itk::ImageIOBase::SHORT:{        // int16_t
    typedef short InputComponentType;
    res= dispatch_pT<InputComponentType>(pixelType, dimensionType, argc, argv);
  } break;
  case itk::ImageIOBase::UINT:{         // uint32_t
    typedef unsigned int InputComponentType;
    res= dispatch_pT<InputComponentType>(pixelType, dimensionType, argc, argv);
  } break;
  case itk::ImageIOBase::INT:{          // int32_t
    typedef int InputComponentType;
    res= dispatch_pT<InputComponentType>(pixelType, dimensionType, argc, argv);
  } break;
  case itk::ImageIOBase::ULONG:{        // uint64_t
    typedef unsigned long InputComponentType;
    res= dispatch_pT<InputComponentType>(pixelType, dimensionType, argc, argv);
  } break;
  case itk::ImageIOBase::LONG:{         // int64_t
    typedef long InputComponentType;
    res= dispatch_pT<InputComponentType>(pixelType, dimensionType, argc, argv);
  } break;
  case itk::ImageIOBase::FLOAT:{        // float32
    typedef float InputComponentType;
    res= dispatch_pT<InputComponentType>(pixelType, dimensionType, argc, argv);
  } break;
  case itk::ImageIOBase::DOUBLE:{       // float64
    typedef double InputComponentType;
    res= dispatch_pT<InputComponentType>(pixelType, dimensionType, argc, argv);
  } break;
  case itk::ImageIOBase::UNKNOWNCOMPONENTTYPE:
  default:
    std::cerr << "unknown component type" << std::endl;
    break;
  }//switch
  return res;
}

template<typename InputComponentType>
int dispatch_pT(itk::ImageIOBase::IOPixelType pixelType, size_t dimensionType, int argc, char *argv[]){
  int res= 0;
    //http://www.itk.org/Doxygen45/html/classitk_1_1ImageIOBase.html#abd189f096c2a1b3ea559bc3e4849f658
    //http://www.itk.org/Doxygen45/html/itkImageIOBase_8h_source.html#l00099
    //IOPixelType:: UNKNOWNPIXELTYPE, SCALAR, RGB, RGBA, OFFSET, VECTOR, POINT, COVARIANTVECTOR, SYMMETRICSECONDRANKTENSOR, DIFFUSIONTENSOR3D, COMPLEX, FIXEDARRAY, MATRIX 

  switch (pixelType){
  case itk::ImageIOBase::SCALAR:{
    typedef InputComponentType InputPixelType;
    res= dispatch_D<InputComponentType, InputPixelType>(dimensionType, argc, argv);
  } break;
  case itk::ImageIOBase::RGB:{
    typedef itk::RGBPixel<InputComponentType> InputPixelType;
    res= dispatch_D<InputComponentType, InputPixelType>(dimensionType, argc, argv);
  } break;
  case itk::ImageIOBase::RGBA:{
    typedef itk::RGBAPixel<InputComponentType> InputPixelType;
    res= dispatch_D<InputComponentType, InputPixelType>(dimensionType, argc, argv);
  } break;
  case itk::ImageIOBase::COMPLEX:{
    typedef std::complex<InputComponentType> InputPixelType;
    res= dispatch_D<InputComponentType, InputPixelType>(dimensionType, argc, argv);
  } break;
  case itk::ImageIOBase::VECTOR:{
    typedef itk::VariableLengthVector<InputComponentType> InputPixelType;
    res= dispatch_D<InputComponentType, InputPixelType>(dimensionType, argc, argv);
  } break;
  case itk::ImageIOBase::UNKNOWNPIXELTYPE:
  default:
    std::cerr << std::endl << "Error: Pixel type not handled!" << std::endl;
    break;
  }//switch 
  return res;
}


template<typename InputComponentType, typename InputPixelType>
int dispatch_D(size_t dimensionType, int argc, char *argv[]){
  int res= 0;
  switch (dimensionType){
  // case 1:
  //   res= DoIt<InputComponentType, InputPixelType, 1>(argc, argv);
  //   break;
  // case 2:
  //   res= DoIt<InputComponentType, InputPixelType, 2>(argc, argv);
  //   break;
  case 3:
    res= DoIt<InputComponentType, InputPixelType, 3>(argc, argv);
    break;
  default: 
    std::cerr << "Error: Images of dimension " << dimensionType << " are not handled!" << std::endl;
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
    size_t &dimensionType
    ){
    typedef itk::Image<unsigned char, 3> ImageType;
    itk::ImageFileReader<ImageType>::Pointer imageReader= itk::ImageFileReader<ImageType>::New();
    imageReader->SetFileName(fileName.c_str());
    imageReader->UpdateOutputInformation();

    pixelType = imageReader->GetImageIO()->GetPixelType();
    componentType = imageReader->GetImageIO()->GetComponentType();
    dimensionType= imageReader->GetImageIO()->GetNumberOfDimensions();

    std::cerr << std::endl << "dimensions: " << dimensionType << std::endl;  
    std::cerr << "component type: " << imageReader->GetImageIO()->GetComponentTypeAsString(componentType) << std::endl;
    std::cerr << "component size: " << imageReader->GetImageIO()->GetComponentSize() << std::endl; 
    std::cerr << "pixel type (string): " << imageReader->GetImageIO()->GetPixelTypeAsString(imageReader->GetImageIO()->GetPixelType()) << std::endl; 
    std::cerr << "pixel type: " << pixelType << std::endl << std::endl; 

    }
  


int main(int argc, char *argv[]){
    if ( argc != 4 ){
	std::cerr << "Missing Parameters: "
		  << argv[0]
		  << " Input_Image"
		  << " Output_Image-pattern"
		  << " compress"
    		  << std::endl;

	return EXIT_FAILURE;
	}

    itk::ImageIOBase::IOPixelType pixelType;
    typename itk::ImageIOBase::IOComponentType componentType;
    size_t dimensionType;


    try {
        GetImageType(argv[1], pixelType, componentType, dimensionType);
        }//try
    catch( itk::ExceptionObject &excep){
        std::cerr << argv[0] << ": exception caught !" << std::endl;
        std::cerr << excep << std::endl;
        return EXIT_FAILURE;
        }
 
    return dispatch_cT(componentType, pixelType, dimensionType, argc, argv);
    }






