#include <data/metadata_extension.h>
#include <data/xmipp_image_convert.h>
#include <iostream>
#include "../../../external/gtest-1.6.0/fused-src/gtest/gtest.h"
#include <stdlib.h>
#include <string.h>
/*
 * Define a "Fixure so we may reuse the metadatas
 */
class MetadataTest : public ::testing::Test
{
protected:
    //init metadatas

    virtual void SetUp()
    {
        chdir(((String)(getXmippPath() + (String)"/resources/test")).c_str());
        //Md1
        id = mDsource.addObject();
        mDsource.setValue(MDL_X,1.,id);
        mDsource.setValue(MDL_Y,2.,id);
        id = mDsource.addObject();
        mDsource.setValue(MDL_X,3.,id);
        mDsource.setValue(MDL_Y,4.,id);
        //Mdjoin
        id = mDjoin.addObject();
        mDjoin.setValue(MDL_X,1.,id);
        mDjoin.setValue(MDL_Z,222.,id);
        id = mDjoin.addObject();
        mDjoin.setValue(MDL_X,3.,id);
        mDjoin.setValue(MDL_Z,444.,id);
        //mDanotherSource
        id = mDanotherSource.addObject();
        mDanotherSource.setValue(MDL_X,11.,id);
        mDanotherSource.setValue(MDL_Y,22.,id);
        id = mDanotherSource.addObject();
        mDanotherSource.setValue(MDL_X,33.,id);
        mDanotherSource.setValue(MDL_Y,44.,id);

        //Md UnionAll
        mDunion = mDsource;
        id1 = mDunion.addObject();
        mDunion.setValue(MDL_X,11.,id1);
        mDunion.setValue(MDL_Y,22.,id1);
        id2 = mDunion.addObject();
        mDunion.setValue(MDL_X,33.,id2);
        mDunion.setValue(MDL_Y,44.,id2);
    }

    // virtual void TearDown() {}//Destructor

    MetaData mDsource,mDanotherSource;
    MetaData mDunion, mDjoin;
    size_t id, id1,id2;
};

TEST_F( MetadataTest, SimilarToOperator)
{
    ASSERT_EQ(mDsource,mDsource);
    ASSERT_FALSE(mDsource==mDanotherSource);
    //attribute order should not be important
    MetaData auxMetadata ;
    id = auxMetadata.addObject();
    auxMetadata.setValue(MDL_Y,2.,id);
    auxMetadata.setValue(MDL_X,1.,id);
    id = auxMetadata.addObject();
    auxMetadata.setValue(MDL_Y,4.,id);
    auxMetadata.setValue(MDL_X,3.,id);
    ASSERT_EQ(auxMetadata,mDsource);
    //Test form double with a given precission
    auxMetadata.clear();
    auxMetadata.setPrecission(2);
    id = auxMetadata.addObject();
    auxMetadata.setValue(MDL_Y,2.001,id);
    auxMetadata.setValue(MDL_X,1.,id);
    id = auxMetadata.addObject();
    auxMetadata.setValue(MDL_Y,4.,id);
    auxMetadata.setValue(MDL_X,3.,id);
    ASSERT_TRUE(auxMetadata==mDsource);
    auxMetadata.setPrecission(4);
    ASSERT_FALSE(auxMetadata==mDsource);


}
/** SORT FOR ROUTINE ALPHABETIC ORDER
 *
 */


TEST_F( MetadataTest, AddLabel)
{
    MetaData auxMetadata = mDunion;
    auxMetadata.addLabel(MDL_Z);
    std::vector<MDLabel> v1,v2;
    v1.push_back(MDL_X);
    v1.push_back(MDL_Y);
    v1.push_back(MDL_Z);
    v2 = auxMetadata.getActiveLabels();
    EXPECT_EQ(v2,v1);
}

TEST_F( MetadataTest, AddRow)
{
    MetaData md;

    MDRow row;
    row.setValue(MDL_X, 1.);
    row.setValue(MDL_Y, 2.);
    md.addRow(row);
    row.setValue(MDL_X, 3.);
    row.setValue(MDL_Y, 4.);
    md.addRow(row);

    EXPECT_EQ(md, mDsource);
}

TEST_F( MetadataTest, Aggregate1)
{
    //simple agregation
    MetaData md,mdOut;
    size_t count;

    MDRow row;
    row.setValue(MDL_ORDER, (size_t)1);
    row.setValue(MDL_Y, 2.);
    row.setValue(MDL_DEFGROUP, 2);
    md.addRow(row);
    row.setValue(MDL_ORDER, (size_t)1);
    row.setValue(MDL_Y, 4.);
    row.setValue(MDL_DEFGROUP, 23);
    md.addRow(row);

    mdOut.aggregate(md, AGGR_COUNT, MDL_ORDER, MDL_ORDER, MDL_COUNT);
    mdOut.getValue(MDL_COUNT,count,mdOut.firstObject());
    EXPECT_EQ(count, 2);
    mdOut.clear();
    mdOut.aggregate(md, AGGR_COUNT, MDL_Y, MDL_Y, MDL_COUNT);
    mdOut.getValue(MDL_COUNT,count,mdOut.firstObject());
    EXPECT_EQ(count,1);

    MDObject mdValueOut(MDL_Y);
    double d;
    md.aggregateSingle(mdValueOut, AGGR_MAX ,MDL_Y);
    mdValueOut.getValue(d);
    EXPECT_EQ(d,4);

    MDObject mdValueOut2(MDL_ORDER);
    size_t t;
    md.aggregateSingleSizeT(mdValueOut2, AGGR_MAX ,MDL_ORDER);
    mdValueOut2.getValue(t);
    EXPECT_EQ(t,(size_t)1);

    MDObject mdValueOut3(MDL_DEFGROUP);
    int i;
    md.aggregateSingleInt(mdValueOut3, AGGR_MAX ,MDL_DEFGROUP);
    mdValueOut3.getValue(i);
    EXPECT_EQ(i,(int)23);

}

TEST_F( MetadataTest, Aggregate2)
{
    //multiple aggregarion
    MetaData md,mdOut;
    size_t count;

    MDRow row;
    row.setValue(MDL_ORDER, (size_t)1);
    row.setValue(MDL_Y, 2.);
    md.addRow(row);
    row.setValue(MDL_ORDER, (size_t)1);
    row.setValue(MDL_Y, 4.);
    md.addRow(row);
    row.setValue(MDL_ORDER, (size_t)2);
    row.setValue(MDL_Y, 2.);
    md.addRow(row);

    const AggregateOperation MyaggregateOperations[] =
        {
            AGGR_COUNT, AGGR_SUM, AGGR_MIN, AGGR_MAX, AGGR_AVG
        };
    std::vector<AggregateOperation> aggregateOperations(MyaggregateOperations,MyaggregateOperations+5);

    const MDLabel MyoperateLabels[]       =
        {
            MDL_ORDER,MDL_ORDER, MDL_Y, MDL_Y, MDL_Y
        };
    std::vector<MDLabel> operateLabels(MyoperateLabels,MyoperateLabels+5);

    const MDLabel MyresultLabels[]        =
        {
            MDL_ORDER,MDL_COUNT, MDL_SUM,  MDL_MIN, MDL_MAX, MDL_AVG
        };
    std::vector<MDLabel> resultLabels(MyresultLabels,MyresultLabels+6);

    mdOut.aggregate(md,aggregateOperations,operateLabels,resultLabels);
    md.clear();
    row.clear();
    row.setValue(MDL_ORDER, (size_t)1);
    row.setValue(MDL_COUNT, (size_t)2);
    row.setValue(MDL_SUM, 2.);
    row.setValue(MDL_MIN, 2.);
    row.setValue(MDL_MAX, 4.);
    row.setValue(MDL_AVG, 3.);
    md.addRow(row);
    row.setValue(MDL_ORDER, (size_t)2);
    row.setValue(MDL_COUNT, (size_t)1);
    row.setValue(MDL_SUM, 2.);
    row.setValue(MDL_MIN, 2.);
    row.setValue(MDL_MAX, 2.);
    row.setValue(MDL_AVG, 2.);
    md.addRow(row);


    EXPECT_EQ(mdOut,md);
}

TEST_F( MetadataTest, AggregateGroupBy)
{
    //aggregation simple grouped by several attributes
    MetaData md,mdOut;
    size_t count;

    MDRow row;
    row.setValue(MDL_ORDER, (size_t)1);
    row.setValue(MDL_DEFGROUP, 2);
    row.setValue(MDL_Y, 2.);
    md.addRow(row);
    row.setValue(MDL_ORDER, (size_t)1);
    row.setValue(MDL_DEFGROUP, 2);
    row.setValue(MDL_Y, 4.);
    md.addRow(row);
    row.setValue(MDL_ORDER, (size_t)2);
    row.setValue(MDL_DEFGROUP, 2);
    row.setValue(MDL_Y, 2.);
    md.addRow(row);

    const MDLabel myGroupByLabels[] =
        {
            MDL_ORDER, MDL_DEFGROUP
        };
    std::vector<MDLabel> groupbyLabels(myGroupByLabels,myGroupByLabels+2);
    mdOut.aggregateGroupBy(md, AGGR_COUNT, groupbyLabels, MDL_Y, MDL_COUNT);

    md.clear();
    row.clear();
    row.setValue(MDL_ORDER, (size_t)1);
    row.setValue(MDL_DEFGROUP, 2);
    row.setValue(MDL_COUNT, (size_t)2);
    md.addRow(row);
    row.setValue(MDL_ORDER, (size_t)2);
    row.setValue(MDL_DEFGROUP, 2);
    row.setValue(MDL_COUNT, (size_t)1);
    md.addRow(row);

    EXPECT_EQ(mdOut,md);
}

TEST_F( MetadataTest, Clear)
{
    MetaData auxMetadata = mDsource;
    EXPECT_EQ(2,auxMetadata.size());
    auxMetadata.clear();
    EXPECT_EQ(0,auxMetadata.size());
}

TEST_F( MetadataTest, Copy)
{
    MetaData auxMetadata = mDsource;
    EXPECT_EQ(mDsource,auxMetadata);
}

TEST_F( MetadataTest, ReadEmptyBlock)
{
    char sfn[64] = "";
    strncpy(sfn, "/tmp/testGetBlocks_XXXXXX", sizeof sfn);
    mkstemp(sfn);
    MetaData md;
    FileName fn = (String)"block_Empty@"+sfn;
    md.write(fn, MD_OVERWRITE);
    md.clear();
    md.setValue(MDL_IMAGE,(String)"image_data_2_1.xmp",md.addObject());
    md.setValue(MDL_IMAGE,(String)"image_data_2_2.xmp",md.addObject());
    md.write((String)"block_B1@"+sfn,MD_APPEND);

    EXPECT_NO_THROW(MetaData md2(fn););

    unlink(sfn);
}

TEST_F( MetadataTest, GetBlocksInMetadata)
{
    char sfn[64] = "";
    strncpy(sfn, "/tmp/testGetBlocks_XXXXXX", sizeof sfn);
    mkstemp(sfn);

    MetaData auxMetadata;
    auxMetadata.setValue(MDL_IMAGE,(String)"image_1.xmp",auxMetadata.addObject());
    auxMetadata.setValue(MDL_IMAGE,(String)"image_2.xmp",auxMetadata.addObject());
    auxMetadata.write(sfn,MD_OVERWRITE);
    auxMetadata.clear();
    auxMetadata.setValue(MDL_IMAGE,(String)"image_data_1_1.xmp",auxMetadata.addObject());
    auxMetadata.setValue(MDL_IMAGE,(String)"image_data_1_2.xmp",auxMetadata.addObject());
    auxMetadata.write((String)"block_000001@"+sfn,MD_APPEND);
    auxMetadata.clear();
    auxMetadata.setValue(MDL_IMAGE,(String)"image_data_2_1.xmp",auxMetadata.addObject());
    auxMetadata.setValue(MDL_IMAGE,(String)"image_data_2_2.xmp",auxMetadata.addObject());
    auxMetadata.write((String)"block_000002@"+sfn,MD_APPEND);
    auxMetadata.clear();

    StringVector compBlockList;
    compBlockList.push_back("");
    compBlockList.push_back("block_000001");
    compBlockList.push_back("block_000002");

    StringVector readBlockList;
    getBlocksInMetaDataFile(sfn,readBlockList);

    EXPECT_EQ(compBlockList,readBlockList);
    unlink(sfn);
}

TEST_F( MetadataTest, CheckRegularExpression)
{
    char sfn[64] = "";
    strncpy(sfn, "/tmp/testGetBlocks_XXXXXX", sizeof sfn);
    mkstemp(sfn);

    MetaData auxMd, auxMd2;
    auxMd.setValue(MDL_IMAGE,(String)"image_1.xmp",auxMd.addObject());
    auxMd.setValue(MDL_IMAGE,(String)"image_2.xmp",auxMd.addObject());
    auxMd.write(sfn,MD_OVERWRITE);
    auxMd.clear();
    auxMd.setValue(MDL_IMAGE,(String)"image_data_1_1.xmp",auxMd.addObject());
    auxMd.setValue(MDL_IMAGE,(String)"image_data_1_2.xmp",auxMd.addObject());
    auxMd.write((String)"block_000001@"+sfn,MD_APPEND);
    auxMd.clear();
    auxMd.setValue(MDL_IMAGE,(String)"image_data_2_1.xmp",auxMd.addObject());
    auxMd.setValue(MDL_IMAGE,(String)"image_data_2_2.xmp",auxMd.addObject());
    auxMd.write((String)"block_000002@"+sfn,MD_APPEND);
    auxMd.clear();
    auxMd.setValue(MDL_IMAGE,(String)"image_data_3_1.xmp",auxMd.addObject());
    auxMd.setValue(MDL_IMAGE,(String)"image_data_3_2.xmp",auxMd.addObject());
    auxMd.write((String)"block_000003@"+sfn,MD_APPEND);
    auxMd.clear();
    auxMd.setValue(MDL_IMAGE,(String)"image_data_A_1.xmp",auxMd.addObject());
    auxMd.setValue(MDL_IMAGE,(String)"image_data_A_2.xmp",auxMd.addObject());
    auxMd.write((String)"block_A@"+sfn,MD_APPEND);
    auxMd.clear();

    auxMd.clear();
    auxMd.setValue(MDL_IMAGE,(String)"image_data_1_1.xmp",auxMd.addObject());
    auxMd.setValue(MDL_IMAGE,(String)"image_data_1_2.xmp",auxMd.addObject());
    auxMd.setValue(MDL_IMAGE,(String)"image_data_2_1.xmp",auxMd.addObject());
    auxMd.setValue(MDL_IMAGE,(String)"image_data_2_2.xmp",auxMd.addObject());
    auxMd.setValue(MDL_IMAGE,(String)"image_data_3_1.xmp",auxMd.addObject());
    auxMd.setValue(MDL_IMAGE,(String)"image_data_3_2.xmp",auxMd.addObject());

    auxMd2.read((String)"block_000[0-9][0-9][123]@" + sfn);
    EXPECT_EQ(auxMd, auxMd2);

    unlink(sfn);
}

TEST_F( MetadataTest, CheckRegularExpression2)
{
    char sfn[64] = "";
    strncpy(sfn, "/tmp/testGetBlocks_XXXXXX", sizeof sfn);
    mkstemp(sfn);

    MetaData auxMd, auxMd2;
    auxMd.setValue(MDL_IMAGE,(String)"image_1.xmp",auxMd.addObject());
    auxMd.setValue(MDL_IMAGE,(String)"image_2.xmp",auxMd.addObject());
    auxMd.write(sfn,MD_OVERWRITE);
    auxMd.clear();
    auxMd.setValue(MDL_IMAGE,(String)"image_data_1_1.xmp",auxMd.addObject());
    auxMd.setValue(MDL_IMAGE,(String)"image_data_1_2.xmp",auxMd.addObject());
    auxMd.write((String)"block_000001@"+sfn,MD_APPEND);
    auxMd.clear();
    auxMd.setValue(MDL_IMAGE,(String)"image_data_2_1.xmp",auxMd.addObject());
    auxMd.setValue(MDL_IMAGE,(String)"image_data_2_2.xmp",auxMd.addObject());
    auxMd.write((String)"block_000002@"+sfn,MD_APPEND);
    auxMd.clear();
    auxMd.setValue(MDL_IMAGE,(String)"image_data_A_1.xmp",auxMd.addObject());
    auxMd.setValue(MDL_IMAGE,(String)"image_data_A_2.xmp",auxMd.addObject());
    auxMd.write((String)"block_0000023@"+sfn,MD_APPEND);
    auxMd.clear();

    auxMd.clear();
    auxMd.setValue(MDL_IMAGE,(String)"image_data_1_1.xmp",auxMd.addObject());
    auxMd.setValue(MDL_IMAGE,(String)"image_data_1_2.xmp",auxMd.addObject());
    auxMd.setValue(MDL_IMAGE,(String)"image_data_2_1.xmp",auxMd.addObject());
    auxMd.setValue(MDL_IMAGE,(String)"image_data_2_2.xmp",auxMd.addObject());

    auxMd2.read((String)"block_000[0-9][0-9][0-9]$@" + sfn);
    EXPECT_EQ(auxMd, auxMd2);

    unlink(sfn);
}

TEST_F( MetadataTest, ImportObject)
{
    //FIXME importObjects test is in the test named select
    MetaData auxMetadata = mDsource;
    auxMetadata.importObject(mDunion,id1,false);
    auxMetadata.importObject(mDunion,id2,false);
    EXPECT_EQ(auxMetadata,mDunion);
}

TEST_F( MetadataTest, LeftJoin)
{
    MetaData auxMetadata;
    MetaData auxMetadata2 = mDsource;
    auxMetadata2.setValue(MDL_Z,222.,auxMetadata2.firstObject());
    auxMetadata2.setValue(MDL_Z,444.,auxMetadata2.firstObject()+1);//A little bit irregular
    auxMetadata.join(mDsource,mDjoin,MDL_X);
    EXPECT_EQ(auxMetadata,auxMetadata2)<< mDjoin;//print mDjoin if error
}

TEST_F( MetadataTest, InnerJoin1)
{
    MetaData auxMetadata;
    MetaData auxMetadataResult;
    MetaData auxMetadataLeft = mDsource;
    MetaData auxMetadataRight;

    auxMetadataRight.setValue(MDL_Z,1.,auxMetadataRight.firstObject());
    auxMetadataRight.setValue(MDL_ANGLEPSI,11.,auxMetadataRight.firstObject());

    auxMetadata.join(auxMetadataLeft,auxMetadataRight,MDL_X,MDL_Z,INNER);
    auxMetadataResult.setValue(MDL_X,1.,auxMetadataRight.firstObject());
    auxMetadataResult.setValue(MDL_Y,2.,auxMetadataRight.firstObject());
    auxMetadataResult.setValue(MDL_ANGLEPSI,11.,auxMetadataRight.firstObject());

    EXPECT_EQ(auxMetadata,auxMetadataResult)<< mDjoin;//print mDjoin if error
}

TEST_F( MetadataTest, InnerJoin2)
{
    MetaData auxMetadata;
    MetaData auxMetadataResult;
    MetaData auxMetadataLeft = mDsource;
    MetaData auxMetadataRight;

    auxMetadataRight.setValue(MDL_Z,1.,auxMetadataRight.firstObject());
    auxMetadataRight.setValue(MDL_Y,11.,auxMetadataRight.firstObject());

    auxMetadata.join(auxMetadataLeft,auxMetadataRight,MDL_X,MDL_Z,INNER);
    auxMetadataResult.setValue(MDL_X,1.,auxMetadataRight.firstObject());
    auxMetadataResult.setValue(MDL_Y,2.,auxMetadataRight.firstObject());

    EXPECT_EQ(auxMetadata,auxMetadataResult)<< mDjoin;//print mDjoin if error
}

TEST_F( MetadataTest, Intersect)
{
    MetaData auxMetadata = mDunion;
    auxMetadata.intersection(mDsource,MDL_X);
    EXPECT_EQ(auxMetadata,mDsource);
}

TEST_F( MetadataTest, Merge)
{
    //FIXME is columns not in the same order equal to operator does not return OK
    //should not be like this
    MetaData auxMetadata3, auxMetadata,auxMetadata2;
    id = auxMetadata3.addObject();
    auxMetadata3.setValue(MDL_Z,222.,id);
    id = auxMetadata3.addObject();
    auxMetadata3.setValue(MDL_Z,444.,id);
    auxMetadata.join(mDsource,mDjoin,MDL_X);
    auxMetadata2 = mDsource;
    auxMetadata2.merge(auxMetadata3);
    EXPECT_EQ(auxMetadata,auxMetadata2);
}


TEST_F( MetadataTest, MultiQuery)
{
    MetaData auxMetadata;
    MetaData auxMetadata3;
    id = auxMetadata3.addObject();
    auxMetadata3.setValue(MDL_X,1.,id);
    auxMetadata3.setValue(MDL_Y,2.,id);
    auxMetadata3.setValue(MDL_Z,222.,id);
    id = auxMetadata3.addObject();
    auxMetadata3.setValue(MDL_X,3.,id);
    auxMetadata3.setValue(MDL_Y,4.,id);
    auxMetadata3.setValue(MDL_Z,333.,id);
    id = auxMetadata3.addObject();
    auxMetadata3.setValue(MDL_X,3.,id);
    auxMetadata3.setValue(MDL_Y,4.,id);
    auxMetadata3.setValue(MDL_Z,444.,id);

    MDValueEQ eq1(MDL_X, 3.);
    MDValueEQ eq2(MDL_Y, 4.);
    MDMultiQuery multi;

    //Test empty query
    auxMetadata.importObjects(auxMetadata3, multi);
    EXPECT_EQ(auxMetadata3,auxMetadata);

    multi.addAndQuery(eq1);
    multi.addAndQuery(eq2);

    auxMetadata.importObjects(auxMetadata3, multi);

    MetaData outMetadata;
    id = outMetadata.addObject();
    outMetadata.setValue(MDL_X,3.,id);
    outMetadata.setValue(MDL_Y,4.,id);
    outMetadata.setValue(MDL_Z,333.,id);
    id = outMetadata.addObject();
    outMetadata.setValue(MDL_X,3.,id);
    outMetadata.setValue(MDL_Y,4.,id);
    outMetadata.setValue(MDL_Z,444.,id);

    EXPECT_EQ(outMetadata,auxMetadata);
}

TEST_F( MetadataTest, NaturalJoin)
{
    MetaData auxMetadata;
    MetaData auxMetadata3;
    id = auxMetadata3.addObject();
    auxMetadata3.setValue(MDL_X,1.,id);
    auxMetadata3.setValue(MDL_Y,2.,id);
    auxMetadata3.setValue(MDL_Z,222.,id);
    id = auxMetadata3.addObject();
    auxMetadata3.setValue(MDL_X,3.,id);
    auxMetadata3.setValue(MDL_Y,4.,id);
    auxMetadata3.setValue(MDL_Z,333.,id);
    id = auxMetadata3.addObject();
    auxMetadata3.setValue(MDL_X,5.,id);
    auxMetadata3.setValue(MDL_Y,6.,id);
    auxMetadata3.setValue(MDL_Z,444.,id);

    auxMetadata.join(mDsource,auxMetadata3,MDL_X,NATURAL);
    auxMetadata3.removeObject(id);
    EXPECT_EQ(auxMetadata,auxMetadata3);
}

TEST_F( MetadataTest, Operate)
{
    MetaData auxMetadata = mDunion;
    MetaData auxMetadata2 = mDunion;
    auxMetadata.operate((String)"X=2*X");
    double x;
    FOR_ALL_OBJECTS_IN_METADATA(auxMetadata2)
    {
        auxMetadata2.getValue(MDL_X,x,__iter.objId);
        auxMetadata2.setValue(MDL_X,x*2,__iter.objId);
    }

    EXPECT_EQ(auxMetadata,auxMetadata2);
}

TEST_F( MetadataTest, Query)
{
    MetaData auxMetadata;
    MetaData auxMetadata3;
    id = auxMetadata3.addObject();
    auxMetadata3.setValue(MDL_X,1.,id);
    auxMetadata3.setValue(MDL_Y,2.,id);
    auxMetadata3.setValue(MDL_Z,222.,id);
    id = auxMetadata3.addObject();
    auxMetadata3.setValue(MDL_X,3.,id);
    auxMetadata3.setValue(MDL_Y,4.,id);
    auxMetadata3.setValue(MDL_Z,333.,id);
    id = auxMetadata3.addObject();
    auxMetadata3.setValue(MDL_X,3.,id);
    auxMetadata3.setValue(MDL_Y,4.,id);
    auxMetadata3.setValue(MDL_Z,444.,id);

    auxMetadata.importObjects(auxMetadata3, MDValueEQ(MDL_X, 3.));

    MetaData outMetadata;
    id = outMetadata.addObject();
    outMetadata.setValue(MDL_X,3.,id);
    outMetadata.setValue(MDL_Y,4.,id);
    outMetadata.setValue(MDL_Z,333.,id);
    id = outMetadata.addObject();
    outMetadata.setValue(MDL_X,3.,id);
    outMetadata.setValue(MDL_Y,4.,id);
    outMetadata.setValue(MDL_Z,444.,id);

    EXPECT_EQ(outMetadata,auxMetadata);
}


TEST_F( MetadataTest, Randomize)
{
    int different,equal;
    different=-1;
    equal=-2;
    MetaData auxMetadata;
    for (int var = 0; var < 20; var++)
    {
        auxMetadata.randomize(mDsource);
        if(mDsource==auxMetadata)
            equal=1;
        else
            different=1;
    }
    EXPECT_EQ(different,equal);
}

TEST_F( MetadataTest, ReadMultipleBlocks)
{
    char sfn[64] = "";
    strncpy(sfn, "/tmp/testReadMultipleBlocks_XXXXXX", sizeof sfn);
    mkstemp(sfn);

    MetaData auxMetadata;
    auxMetadata.setValue(MDL_IMAGE,(String)"image_1.xmp",auxMetadata.addObject());
    auxMetadata.setValue(MDL_IMAGE,(String)"image_2.xmp",auxMetadata.addObject());
    auxMetadata.write(sfn,MD_OVERWRITE);
    auxMetadata.clear();
    auxMetadata.setValue(MDL_IMAGE,(String)"image_data_1_1.xmp",auxMetadata.addObject());
    auxMetadata.setValue(MDL_IMAGE,(String)"image_data_1_2.xmp",auxMetadata.addObject());
    auxMetadata.write((String)"block_000001@"+sfn,MD_APPEND);
    auxMetadata.clear();
    auxMetadata.setValue(MDL_IMAGE,(String)"image_data_2_1.xmp",auxMetadata.addObject());
    auxMetadata.setValue(MDL_IMAGE,(String)"image_data_2_2.xmp",auxMetadata.addObject());
    auxMetadata.write((String)"block_000002@"+sfn,MD_APPEND);
    auxMetadata.clear();
    auxMetadata.setValue(MDL_IMAGE,(String)"image_data_no_1.xmp",auxMetadata.addObject());
    auxMetadata.setValue(MDL_IMAGE,(String)"image_data_no_2.xmp",auxMetadata.addObject());
    auxMetadata.write((String)"noblock@"+sfn,MD_APPEND);
    auxMetadata.clear();
    auxMetadata.setValue(MDL_IMAGE,(String)"image_data_3_1.xmp",auxMetadata.addObject());
    auxMetadata.setValue(MDL_IMAGE,(String)"image_data_3_2.xmp",auxMetadata.addObject());
    auxMetadata.write((String)"block_000003@"+sfn,MD_APPEND);
    auxMetadata.clear();

    MetaData compMetadata;
    compMetadata.setValue(MDL_IMAGE,(String)"image_data_1_1.xmp",compMetadata.addObject());
    compMetadata.setValue(MDL_IMAGE,(String)"image_data_1_2.xmp",compMetadata.addObject());
    compMetadata.setValue(MDL_IMAGE,(String)"image_data_2_1.xmp",compMetadata.addObject());
    compMetadata.setValue(MDL_IMAGE,(String)"image_data_2_2.xmp",compMetadata.addObject());
    auxMetadata.read((String)"block_00000[12]@"+sfn);
    EXPECT_EQ(compMetadata,auxMetadata);
    compMetadata.clear();

    compMetadata.setValue(MDL_IMAGE,(String)"image_data_3_1.xmp",compMetadata.addObject());
    compMetadata.setValue(MDL_IMAGE,(String)"image_data_3_2.xmp",compMetadata.addObject());
    auxMetadata.read((String)"block_000003@"+sfn);
    EXPECT_EQ(compMetadata,auxMetadata);
    compMetadata.clear();

    compMetadata.setValue(MDL_IMAGE,(String)"image_1.xmp",compMetadata.addObject());
    compMetadata.setValue(MDL_IMAGE,(String)"image_2.xmp",compMetadata.addObject());
    auxMetadata.read(sfn);
    EXPECT_EQ(compMetadata,auxMetadata);
    compMetadata.clear();
    unlink(sfn);
}

TEST_F( MetadataTest, ReadEmptyBlocks)
{
#define sizesfn 64
    char sfn[sizesfn] = "";
    strncpy(sfn, "/tmp/testReadMultipleBlocks_XXXXXX", sizesfn);
    mkstemp(sfn);

    MetaData auxMetadata;
    id=auxMetadata.addObject();
    auxMetadata.setValue(MDL_X,1.,id);
    auxMetadata.setValue(MDL_Y,2.,id);
    auxMetadata.setValue(MDL_Z,222.,id);
    auxMetadata.write((String)"block_000001@"+sfn,MD_APPEND);

    auxMetadata.clear();
    auxMetadata.addLabel(MDL_X);
    auxMetadata.addLabel(MDL_Y);
    auxMetadata.addLabel(MDL_Z);
    auxMetadata.write((String)"block_000002@"+sfn,MD_APPEND);

    auxMetadata.clear();
    id=auxMetadata.addObject();
    auxMetadata.setValue(MDL_X,1.,id);
    auxMetadata.setValue(MDL_Y,2.,id);
    auxMetadata.setValue(MDL_Z,222.,id);
    auxMetadata.write((String)"block_000003@"+sfn,MD_APPEND);

    auxMetadata.clear();
    auxMetadata.addLabel(MDL_X);
    auxMetadata.addLabel(MDL_Y);
    auxMetadata.addLabel(MDL_Z);
    auxMetadata.write((String)"block_000004@"+sfn,MD_APPEND);

    auxMetadata.read((String)"block_000002@"+sfn);
    EXPECT_EQ(auxMetadata.size(),0);

    auxMetadata.read((String)"block_000004@"+sfn);
    EXPECT_EQ(auxMetadata.size(),0);

    unlink(sfn);
}

TEST_F( MetadataTest, ReadEmptyBlocksII)
{
    char sfn[64] = "";
    strncpy(sfn, "/tmp/testReadMultipleBlocks_XXXXXX", sizeof sfn);
    mkstemp(sfn);

    MetaData auxMetadata;

    auxMetadata.addLabel(MDL_X);
    auxMetadata.addLabel(MDL_Y);
    auxMetadata.addLabel(MDL_Z);
    auxMetadata.write((String)"block_000002@"+sfn,MD_APPEND);

    auxMetadata.read((String)"block_000002@"+sfn);
    EXPECT_EQ(auxMetadata.size(),0);
    unlink(sfn);
}

TEST_F( MetadataTest, ReadWrite)
{
    //temp file name
    char sfn[32] = "";
    strncpy(sfn, "/tmp/testWrite_XXXXXX", sizeof sfn);
    mkstemp(sfn);
    mDsource.write(sfn);

    MetaData auxMetadata;
    auxMetadata.read(sfn);

    EXPECT_EQ(mDsource,auxMetadata);
    unlink(sfn);
}

TEST_F( MetadataTest, ExistsBlock)
{
    //temp file name
    char sfn[32] = "";
    strncpy(sfn, "/tmp/testWrite_XXXXXX", sizeof sfn);
    mkstemp(sfn);
    FileName tmpFileName((String) "kk@" + sfn);
    mDsource.write(tmpFileName);
    MetaData auxMetadata;
    bool result1 = auxMetadata.existsBlock(tmpFileName);
    EXPECT_EQ(result1,true);
    tmpFileName =(String) "kk2@" + sfn;
    result1 = auxMetadata.existsBlock(tmpFileName);
    EXPECT_EQ(result1,false);
    unlink(sfn);
}

TEST_F( MetadataTest, ReadWriteAppendBlock)
{
    XMIPP_TRY
    //temp file name
    char sfn[32] = "";
    strncpy(sfn, "/tmp/testWrite_XXXXXX", sizeof sfn);
    mkstemp(sfn);
    mDsource.write((String)"one@"+sfn);
    mDsource.write((String)"two@"+sfn,MD_APPEND);
    mDsource.write((String)"three@"+sfn,MD_APPEND);
    MetaData auxMetadata;
    FileName sfn2 = "metadata/ReadWriteAppendBlock.xmd";
    auxMetadata.read(sfn);
    EXPECT_TRUE(compareTwoFiles(sfn,sfn2,0));
    unlink(sfn);
    XMIPP_CATCH
}

TEST_F( MetadataTest, RemoveDuplicates)
{
    MetaData auxMetadata1,auxMetadata3;
    id = auxMetadata3.addObject();
    auxMetadata3.setValue(MDL_X,1.,id);
    auxMetadata3.setValue(MDL_Y,2.,id);
    id = auxMetadata3.addObject();
    auxMetadata3.setValue(MDL_X,3.,id);
    auxMetadata3.setValue(MDL_Y,4.,id);
    id = auxMetadata3.addObject();
    auxMetadata3.setValue(MDL_X,1.,id);
    auxMetadata3.setValue(MDL_Y,2.,id);
    auxMetadata1.removeDuplicates(auxMetadata3);
    EXPECT_EQ(auxMetadata1,mDsource);//print mDjoin if error
}

TEST_F( MetadataTest, Removelabel)
{
    MetaData auxMetadata = mDunion;
    auxMetadata.removeLabel(MDL_X);
    std::vector<MDLabel> v1,v2;
    v1.push_back(MDL_Y);
    v2 = auxMetadata.getActiveLabels();
    EXPECT_EQ(v2,v1);
}

TEST_F( MetadataTest, Select)
{
    MetaData auxMetadata;
    MetaData auxMetadata2;
    id = auxMetadata2.addObject();
    auxMetadata2.setValue(MDL_X,3.,id);
    auxMetadata2.setValue(MDL_Y,4.,id);

    auxMetadata.importObjects(mDsource,MDExpression((String)"x>2"));
    EXPECT_EQ(auxMetadata,auxMetadata2);
}


TEST_F( MetadataTest, Size)
{
    EXPECT_EQ(2, mDsource.size());
}

TEST_F( MetadataTest, Sort)
{
    MetaData auxMetadata,auxMetadata2,auxMetadata3,outMetadata;
    id = auxMetadata.addObject();
    auxMetadata.setValue(MDL_X,3.,id);
    auxMetadata.setValue(MDL_Y,4.,id);
    id = auxMetadata.addObject();
    auxMetadata.setValue(MDL_X,1.,id);
    auxMetadata.setValue(MDL_Y,2.,id);
    auxMetadata2.sort(auxMetadata,MDL_X);
    EXPECT_EQ(auxMetadata2,mDsource);

    id = auxMetadata.addObject();
    auxMetadata.setValue(MDL_X,5.,id);
    auxMetadata.setValue(MDL_Y,6.,id);

    auxMetadata2.clear();
    auxMetadata2.sort(auxMetadata,MDL_X,true,1,0);
    id = outMetadata.addObject();
    outMetadata.setValue(MDL_X,1.,id);
    outMetadata.setValue(MDL_Y,2.,id);
    EXPECT_EQ(auxMetadata2,outMetadata);

    auxMetadata2.clear();
    auxMetadata2.sort(auxMetadata,MDL_X,true,2,1);
    outMetadata.clear();
    id = outMetadata.addObject();
    outMetadata.setValue(MDL_X,3.,id);
    outMetadata.setValue(MDL_Y,4.,id);
    id = outMetadata.addObject();
    outMetadata.setValue(MDL_X,5.,id);
    outMetadata.setValue(MDL_Y,6.,id);
    EXPECT_EQ(auxMetadata2,outMetadata);
}

TEST_F( MetadataTest, Substraction)
{
    MetaData auxMetadata = mDunion;
    auxMetadata.subtraction(mDanotherSource,MDL_X);
    EXPECT_EQ(auxMetadata,mDsource);
}

TEST_F( MetadataTest, Union)
{
    //FIXME union all is missing
    MetaData auxMetadata = mDsource;
    auxMetadata.unionAll(mDanotherSource);
    EXPECT_EQ(auxMetadata,mDunion);
}

//check if mdl label match its type and
//check if int is different from size_t
TEST_F( MetadataTest, setGetValue)
{
    size_t t;
    int i;
    EXPECT_EQ(MDL::labelType(MDL_ORDER),LABEL_LONG);
    MetaData auxMetadata;
    id = auxMetadata.addObject();
    auxMetadata.setValue(MDL_ORDER,(size_t)1, id);
    auxMetadata.getValue(MDL_ORDER,t, id);
    EXPECT_EQ((size_t)1,t);
    //We expect that MetaData will throw an exception
    //if you use getValue with a variable of type that
    // doesn't match the label type
    EXPECT_THROW(auxMetadata.getValue(MDL_ORDER, i, id), XmippError);
}

//read file with vector
TEST_F( MetadataTest, getValue)
{
    XMIPP_TRY
    std::vector<double> v1(3);
    std::vector<double> v2(3);
    MetaData auxMD1;
    id = auxMD1.addObject();
    v1[0]=1.;
    v1[1]=2.;
    v1[2]=3.;
    auxMD1.setValue(MDL_ANGLE_COMPARISON,v1,id);
    id = auxMD1.firstObject();
    auxMD1.getValue(MDL_ANGLE_COMPARISON,v2, id);

    EXPECT_EQ(v1[0],v2[0]);
    EXPECT_EQ(v1[1],v2[1]);
    EXPECT_EQ(v1[2],v2[2]);
    XMIPP_CATCH
}
//Copy images on metadata using ImageConvert logic
TEST_F( MetadataTest, copyImages)
{
    XMIPP_TRY
    FileName fn = "metadata/smallStack.stk";
    FileName out;
    FileName oroot;
    oroot.initUniqueName("/tmp/smallImg_XXXXXX");
    oroot.deleteFile();
    out = oroot.addExtension("xmd");
    oroot = oroot + ":mrc";

    FileName fn1, fn2;
    MetaData md(fn);
    ProgConvImg conv;
    conv.verbose = 0;
    conv.setup(&md, out, oroot);
    conv.tryRun();
    MetaData *mdOut = conv.getOutputMd();

    FOR_ALL_OBJECTS_IN_METADATA2(md, *mdOut)
    {
        md.getValue(MDL_IMAGE, fn1, __iter.objId);
        mdOut->getValue(MDL_IMAGE, fn2, __iter2.objId);
        EXPECT_TRUE(compareImage(fn1, fn2));
        fn2.deleteFile();
    }

    out.deleteFile();
    out.initUniqueName("/tmp/smallStack_XXXXXX");
    out = out + ":mrcs";
    conv.setup(&md, out);
    conv.tryRun();

    FOR_ALL_OBJECTS_IN_METADATA2(md, *mdOut)
    {
        md.getValue(MDL_IMAGE, fn1, __iter.objId);
        mdOut->getValue(MDL_IMAGE, fn2, __iter2.objId);
        EXPECT_TRUE(compareImage(fn1, fn2));
    }
    out.deleteFile();

    out.initUniqueName("/tmp/smallStackVol_XXXXXX");
    out = out + ":mrc";
    conv.setup(&md, out);
    conv.tryRun();
    Image<float> imgStk, imgVol;
    imgStk.read(fn);
    imgVol.read(out);

    int n = imgStk.getDimensions().ndim;
    for (int i = FIRST_IMAGE; i <= n; ++i)
    {
        imgStk.movePointerTo(1, i);
        imgVol.movePointerTo(i);
        EXPECT_TRUE(imgStk == imgVol);
    }
    out.deleteFile();
    XMIPP_CATCH
}

GTEST_API_ int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
