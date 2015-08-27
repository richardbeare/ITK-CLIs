////program to convert a voxel representation of a simplical 2-complex (e.g. voxel-skeleton consisting only of vertices and lines) into a vector/graph representation
//01: based on count_neighbours.cxx


#include <complex>

#include "itkFilterWatcher.h"
#include <itkImageFileReader.h>
#include "filter/external/itkCountNeighborsImageFilter/itkCountNeighborsImageFilter.h"
#include <itkChangeLabelImageFilter.h>
#include <itkBinaryThresholdImageFilter.h>
#include <itkBinaryImageToShapeLabelMapFilter.h>
#include <itkShapeLabelObject.h>
#include <itkLabelMap.h>
#include <itkLabelMapToLabelImageFilter.h>

#include <itkMesh.h>
#include <itkLineCell.h>
#include <itkMeshFileWriter.h>
#include <itkVTKPolyDataMeshIO.h>
#include <itkMetaDataObject.h>



template<typename InputComponentType, typename InputPixelType, size_t Dimension>
int DoIt(int argc, char *argv[]){

    typedef uint8_t  OutputPixelType;
    typedef uint32_t  LabelPixelType;

    typedef itk::Image<InputPixelType, Dimension>  InputImageType;
    typedef itk::Image<LabelPixelType, Dimension>  LabelImageType;
    typedef itk::Image<OutputPixelType, Dimension>  OutputImageType;


    typedef itk::ImageFileReader<InputImageType> ReaderType;
    typename ReaderType::Pointer reader = ReaderType::New();

    reader->SetFileName(argv[1]);
    reader->ReleaseDataFlagOn();
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

    const typename InputImageType::Pointer& input= reader->GetOutput();


    typename InputImageType::SizeType radius;
    radius.Fill(1);//only regard 26 connectivity

    typedef itk::CountNeighborsImageFilter<InputImageType, OutputImageType> FilterType;
    typename FilterType::Pointer filter= FilterType::New();
    filter->SetInput(input);
    filter->SetRadius(radius);
    filter->SetCountNonZero();
    filter->SetValueOfInterest(static_cast<InputPixelType>(atof(argv[3])));
    //filter->ReleaseDataFlagOn();//will be needed multiple times!
    //filter->InPlaceOn();//not available

    FilterWatcher watcher1(filter);
    try{
        filter->Update();
        }
    catch(itk::ExceptionObject &ex){
        std::cerr << ex << std::endl;
        return EXIT_FAILURE;
        }


    const typename OutputImageType::Pointer& output= filter->GetOutput();


    const OutputPixelType fg= 1;
    const OutputPixelType bg= 0;

    //// get all verts that are neither end-nodes (NN=1) nor primitive connection nodes (NN=2)
    typedef itk::ChangeLabelImageFilter<OutputImageType, OutputImageType> ChangeLabType;
    typename ChangeLabType::Pointer ch= ChangeLabType::New();
    ch->SetInput(filter->GetOutput());
    ch->SetChange(2, 0);//delete connecting nodes of branches

    typedef itk::BinaryThresholdImageFilter<OutputImageType, OutputImageType> ThrFilterType;
    typename ThrFilterType::Pointer thr= ThrFilterType::New();
    thr->SetInput(ch->GetOutput());
    thr->SetLowerThreshold(1);//all non-connecting nodes of any degree
    thr->SetOutsideValue(bg);
    thr->SetInsideValue(fg);
    thr->ReleaseDataFlagOn();
    FilterWatcher watcherThr(thr);

    typedef itk::BinaryImageToShapeLabelMapFilter<OutputImageType> AnaFilterType;
    typename AnaFilterType::Pointer ana= AnaFilterType::New();
    ana->SetInput(thr->GetOutput());
    ana->SetInputForegroundValue(fg);
    ana->FullyConnectedOn();//expecting skeletons generated by itkBinaryThinningImageFilter3D which are not fully connected so fully connectivity is needed here
    ana->ReleaseDataFlagOn();

    FilterWatcher watcher2(ana);
    try{
        ana->Update();
        }
    catch(itk::ExceptionObject &ex){
        std::cerr << ex << std::endl;
        return EXIT_FAILURE;
        }

    //// create mesh to save in a VTK-file
    typedef typename itk::Mesh<float, Dimension>  MeshType;
    typename MeshType::Pointer  mesh = MeshType::New();

    typedef typename AnaFilterType::OutputImageType LabelMapType;
    typedef typename AnaFilterType::OutputImageType::LabelObjectType LabelObjectType;
    typedef typename AnaFilterType::OutputImageType::LabelType LabelType;//default: SizeValueType

    typename LabelMapType::Pointer labelMap = ana->GetOutput();
    const LabelObjectType* labelObject;

    for(LabelType label= 0; label < labelMap->GetNumberOfLabelObjects(); label++){

        labelObject= labelMap->GetNthLabelObject(label);
        typename LabelObjectType::ConstIndexIterator lit(labelObject);

        mesh->SetPoint(label, labelObject->GetCentroid());
        mesh->SetPointData(label, output->GetPixel(lit.GetIndex()));//make sure filter->ReleaseDataFlagOn() is NOT set!
        }

    typename MeshType::PointIdentifier pointIndex= mesh->GetNumberOfPoints();
    std::cerr << "# of mesh points: " << mesh->GetNumberOfPoints() << std::endl;

    typename LabelImageType::Pointer bpi;
	{//scoped for better consistency
	typedef itk::LabelMapToLabelImageFilter<LabelMapType, LabelImageType> LMtLIType;
	typename LMtLIType::Pointer lmtli = LMtLIType::New();
	lmtli->SetInput(labelMap);
	FilterWatcher watcher3(lmtli);
	try{
	    lmtli->Update();
	    }
	catch(itk::ExceptionObject &ex){
	    std::cerr << ex << std::endl;
	    return EXIT_FAILURE;
	    }
	bpi= lmtli->GetOutput();
	bpi->DisconnectPipeline();
	}


    //// create connecting lines
    thr->SetLowerThreshold(2);//only connecting nodes of branches
    thr->SetUpperThreshold(2);//only connecting nodes of branches

    try{
        thr->Update();
        }
    catch(itk::ExceptionObject &ex){
        std::cerr << ex << std::endl;
        return EXIT_FAILURE;
        }

    typename OutputImageType::Pointer thrimg= thr->GetOutput();
    thrimg->DisconnectPipeline();

    try{
        ana->Update();//also updates thr
        }
    catch(itk::ExceptionObject &ex){
        std::cerr << ex << std::endl;
        return EXIT_FAILURE;
        }

    typedef typename MeshType::CellType CellType;
    typedef typename itk::LineCell<CellType> LineType;
    typedef typename CellType::CellAutoPointer  CellAutoPointer;

    size_t cellId=0;
    typename MeshType::PointType mP;
    for(LabelType label= 0; label < labelMap->GetNumberOfLabelObjects(); label++){

        labelObject= labelMap->GetNthLabelObject(label);
        typename LabelObjectType::ConstIndexIterator lit(labelObject);
	
	typename LabelImageType::RegionType region= bpi->GetLargestPossibleRegion();
	typedef itk::ConstNeighborhoodIterator<LabelImageType> NeighborhoodIteratorType;
	typename NeighborhoodIteratorType::RadiusType radius;
	radius.Fill(1); //26-connectivity
	NeighborhoodIteratorType nit(radius, bpi, region);

	typedef itk::ConstNeighborhoodIterator<OutputImageType> ThrNeighborhoodIteratorType;
	ThrNeighborhoodIteratorType tit(radius, thrimg, region);
	tit.SetLocation(lit.GetIndex());//lit initial position can be on any pixel of the label
	typename ThrNeighborhoodIteratorType::IndexType lastIndex;

	////find one branch end
	bool foundEnd= false;
        std::cerr << "searching for one end... ";
	while(!foundEnd){
	    foundEnd=true;
	    typename ThrNeighborhoodIteratorType::OffsetType nextMove;
	    nextMove.Fill(0);
	    for(unsigned int i = 0; i < tit.Size(); ++i){
		if(i == tit.GetCenterNeighborhoodIndex()) 
		    continue;//skips center pixel
		if(tit.GetIndex(i) == lastIndex)
		    continue;//skips last visited pixel
		if(tit.GetPixel(i)){
		    nextMove= tit.GetOffset(i);
		    foundEnd=false;//this is not an end pixel
		    }
		}
	    lastIndex= tit.GetIndex();//it.GetIndex() == it.GetIndex(it.GetCenterNeighborhoodIndex())
	    tit+= nextMove;
	    }
        std::cerr << "found one end";


	nit.SetLocation(tit.GetIndex());//only need neighbourhood of end pixel
	LabelPixelType v;
	for(unsigned int i = 0; i < nit.Size(); ++i){
	    if(i == nit.GetCenterNeighborhoodIndex()) 
		continue;//skips center pixel
	    if(v= nit.GetPixel(i)) 
		break;
	    }
        std::cerr << " connected to bp: " << v << std::endl;

	//typename MeshType::PointIdentifier pPointIndex= v;//join with bp, bpi value > 0 is index
	typename MeshType::PointIdentifier pPointIndex= pointIndex;//do not join with bp
	bool foundOtherEnd= false;
	while(!foundOtherEnd){

	    labelMap->TransformIndexToPhysicalPoint(tit.GetIndex(), mP);
	    mesh->SetPoint(pointIndex, mP);
	    mesh->SetPointData(pointIndex, 2);//only connecting nodes of branches

	    typename CellType::CellAutoPointer line;
	    line.TakeOwnership(new LineType);
	    line->SetPointId(0, pPointIndex);
	    line->SetPointId(1, pointIndex);
	    mesh->SetCell(cellId, line);
	    //mesh->SetCellData(cellId, label);//this would be logical but turns out to  be wrong if VTK-file is loaded by e.g. paraview

	    pPointIndex= pointIndex;
	    pointIndex++;
	    cellId++;

	    foundOtherEnd=true;
	    typename ThrNeighborhoodIteratorType::OffsetType nextMove;
	    nextMove.Fill(0);
	    for(unsigned int i = 0; i < tit.Size(); ++i){
		if(i == tit.GetCenterNeighborhoodIndex()) 
		    continue;//skips center pixel
		if(tit.GetIndex(i) == lastIndex) 
		    continue;//skips last visited pixel
		if(tit.GetPixel(i)){
		    nextMove = tit.GetOffset(i);
		    foundOtherEnd=false;//this is not an end pixel
		    }
		}
	    lastIndex= tit.GetIndex();//it.GetIndex() == it.GetIndex(it.GetCenterNeighborhoodIndex())
	    tit+= nextMove;
	    }
        std::cerr << "found other end";

	////connect other end of branch poly-line to its branch point
	nit.SetLocation(tit.GetIndex());//only need neighbourhood of end pixel
	for(unsigned int i = 0; i < nit.Size(); ++i){
	    if(i == nit.GetCenterNeighborhoodIndex()) 
		continue;//skips center pixel
	    if(v= nit.GetPixel(i)) 
		break;
	    }
        std::cerr << " connected to bp: " << v << std::endl;

	mesh->SetCellData(label, label);//oddity of ITK-mesh: consecutive line-cells are joined to form a single polyline-cell
	}

    std::cerr << "# of mesh cells: " << mesh->GetNumberOfCells() << std::endl;
    std::cerr << "cellIDs: " << cellId << std::endl;
    std::cerr << "labels: " << labelMap->GetNumberOfLabelObjects() << std::endl;

    typedef typename itk::MeshFileWriter<MeshType> MeshWriterType;
    typename MeshWriterType::Pointer mwriter = MeshWriterType::New();

    FilterWatcher watcherMO(mwriter);
    mwriter->SetFileName(argv[2]);
    mwriter->SetInput(mesh);

    itk::VTKPolyDataMeshIO::Pointer mio= itk::VTKPolyDataMeshIO::New();
    itk::MetaDataDictionary & metaDic= mio->GetMetaDataDictionary();
    itk::EncapsulateMetaData<std::string>(metaDic, "pointScalarDataName", "Degree");
    itk::EncapsulateMetaData<std::string>(metaDic, "cellScalarDataName", "BranchId");
    mwriter->SetMeshIO(mio);

    try{
        mwriter->Update();
        }
    catch(itk::ExceptionObject &ex){
        std::cerr << ex << std::endl;
        return EXIT_FAILURE;
        }

    return EXIT_SUCCESS;

    }


template<typename InputComponentType, typename InputPixelType>
int dispatch_D(size_t dimensionType, int argc, char *argv[]){
    int res= EXIT_FAILURE;
    switch (dimensionType){
    case 2:
        res= DoIt<InputComponentType, InputPixelType, 2>(argc, argv);
        break;
    case 3:
        res= DoIt<InputComponentType, InputPixelType, 3>(argc, argv);
        break;
    default:
        std::cerr << "Error: Images of dimension " << dimensionType << " are not handled!" << std::endl;
        break;
        }//switch
    return res;
    }

template<typename InputComponentType>
int dispatch_pT(itk::ImageIOBase::IOPixelType pixelType, size_t dimensionType, int argc, char *argv[]){
    int res= EXIT_FAILURE;
    //http://www.itk.org/Doxygen45/html/classitk_1_1ImageIOBase.html#abd189f096c2a1b3ea559bc3e4849f658
    //http://www.itk.org/Doxygen45/html/itkImageIOBase_8h_source.html#l00099
    //IOPixelType:: UNKNOWNPIXELTYPE, SCALAR, RGB, RGBA, OFFSET, VECTOR, POINT, COVARIANTVECTOR, SYMMETRICSECONDRANKTENSOR, DIFFUSIONTENSOR3D, COMPLEX, FIXEDARRAY, MATRIX

    switch (pixelType){
    case itk::ImageIOBase::SCALAR:{
        typedef InputComponentType InputPixelType;
        res= dispatch_D<InputComponentType, InputPixelType>(dimensionType, argc, argv);
        } break;
    case itk::ImageIOBase::UNKNOWNPIXELTYPE:
    default:
        std::cerr << std::endl << "Error: Pixel type not handled!" << std::endl;
        break;
        }//switch
    return res;
    }

int dispatch_cT(itk::ImageIOBase::IOComponentType componentType, itk::ImageIOBase::IOPixelType pixelType, size_t dimensionType, int argc, char *argv[]){
    int res= EXIT_FAILURE;

    //http://www.itk.org/Doxygen45/html/classitk_1_1ImageIOBase.html#a8dc783055a0af6f0a5a26cb080feb178
    //http://www.itk.org/Doxygen45/html/itkImageIOBase_8h_source.html#l00107
    //IOComponentType: UNKNOWNCOMPONENTTYPE, UCHAR, CHAR, USHORT, SHORT, UINT, INT, ULONG, LONG, FLOAT, DOUBLE

    switch (componentType){
    case itk::ImageIOBase::UCHAR:{        // uint8_t
        typedef unsigned char InputComponentType;
        res= dispatch_pT<InputComponentType>(pixelType, dimensionType, argc, argv);
        } break;
    // case itk::ImageIOBase::CHAR:{         // int8_t
    //     typedef char InputComponentType;
    //     res= dispatch_pT<InputComponentType>(pixelType, dimensionType, argc, argv);
    //     } break;
    // case itk::ImageIOBase::USHORT:{       // uint16_t
    //     typedef unsigned short InputComponentType;
    //     res= dispatch_pT<InputComponentType>(pixelType, dimensionType, argc, argv);
    //     } break;
    // case itk::ImageIOBase::SHORT:{        // int16_t
    //     typedef short InputComponentType;
    //     res= dispatch_pT<InputComponentType>(pixelType, dimensionType, argc, argv);
    //     } break;
    // case itk::ImageIOBase::UINT:{         // uint32_t
    //     typedef unsigned int InputComponentType;
    //     res= dispatch_pT<InputComponentType>(pixelType, dimensionType, argc, argv);
    //     } break;
    // case itk::ImageIOBase::INT:{          // int32_t
    //     typedef int InputComponentType;
    //     res= dispatch_pT<InputComponentType>(pixelType, dimensionType, argc, argv);
    //     } break;
    // case itk::ImageIOBase::ULONG:{        // uint64_t
    //     typedef unsigned long InputComponentType;
    //     res= dispatch_pT<InputComponentType>(pixelType, dimensionType, argc, argv);
    //     } break;
    // case itk::ImageIOBase::LONG:{         // int64_t
    //     typedef long InputComponentType;
    //     res= dispatch_pT<InputComponentType>(pixelType, dimensionType, argc, argv);
    //     } break;
    // case itk::ImageIOBase::FLOAT:{        // float32
    //     typedef float InputComponentType;
    //     res= dispatch_pT<InputComponentType>(pixelType, dimensionType, argc, argv);
    //     } break;
    // case itk::ImageIOBase::DOUBLE:{       // float64
    //     typedef double InputComponentType;
    //     res= dispatch_pT<InputComponentType>(pixelType, dimensionType, argc, argv);
    //     } break;
    // case itk::ImageIOBase::UNKNOWNCOMPONENTTYPE:
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
    size_t &dimensionType
    ){
    typedef itk::Image<char, 1> ImageType; //template initialization parameters need to be given but can be arbitrary here
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
                  << " Output_VTK-file"
                  << " fg"
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






