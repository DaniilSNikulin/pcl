/*
 * Software License Agreement (BSD License)
 *
 *  Point Cloud Library (PCL) - www.pointclouds.org
 *  Copyright (c) 2012-, Open Perception, Inc.
 *
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of the copyright holder(s) nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id: test_filters.cpp 7683 2012-10-23 02:49:03Z rusu $
 *
 */

#include <pcl/test/gtest.h>
#include <pcl/pcl_tests.h>

#include <random>

#include <pcl/point_types.h>
#include <pcl/surface/convex_hull.h>
#include <pcl/common/common.h>
#include <pcl/filters/crop_hull.h>


namespace
{
  struct Checker
  {
    pcl::CropHull<pcl::PointXYZ> cropHullFilter;

    Checker(pcl::ConvexHull<pcl::PointXYZ> & hull, int dim)
      : cropHullFilter(true)
    {
      pcl::PointCloud<pcl::PointXYZ>::Ptr hullCloudPtr(new pcl::PointCloud<pcl::PointXYZ>);
      std::vector<pcl::Vertices> hullPolygons;
      hull.reconstruct(*hullCloudPtr, hullPolygons);

      cropHullFilter.setHullIndices(hullPolygons);
      cropHullFilter.setHullCloud(hullCloudPtr);
      cropHullFilter.setDim(dim);
      cropHullFilter.setCropOutside(true);
    }

    void check(
        pcl::Indices const & expectedFilteredIndices,
        pcl::PointCloud<pcl::PointXYZ>::ConstPtr inputCloud)
    {
      std::vector<bool> expectedFilteredMask(inputCloud->size(), false);
      pcl::Indices expectedRemovedIndices;
      pcl::PointCloud<pcl::PointXYZ> expectedCloud;
      pcl::copyPointCloud(*inputCloud, expectedFilteredIndices, expectedCloud);
      for (pcl::index_t idx : expectedFilteredIndices) {
        expectedFilteredMask[idx] = true;
      }
      for (size_t i = 0; i < inputCloud->size(); ++i) {
        if (!expectedFilteredMask[i]) {
          expectedRemovedIndices.push_back(i);
        }
      }

      cropHullFilter.setInputCloud(inputCloud);

      pcl::Indices filteredIndices;
      cropHullFilter.filter(filteredIndices);
      pcl::test::EXPECT_EQ_VECTORS(expectedFilteredIndices, filteredIndices);
      pcl::test::EXPECT_EQ_VECTORS(expectedRemovedIndices, *cropHullFilter.getRemovedIndices());
      // check negative filter functionality
      {
        cropHullFilter.setNegative(true);
        cropHullFilter.filter(filteredIndices);
        pcl::test::EXPECT_EQ_VECTORS(expectedRemovedIndices, filteredIndices);
        pcl::test::EXPECT_EQ_VECTORS(expectedFilteredIndices, *cropHullFilter.getRemovedIndices());
        cropHullFilter.setNegative(false);
      }
      // check cropOutside functionality
      {
        cropHullFilter.setCropOutside(false);
        cropHullFilter.filter(filteredIndices);
        pcl::test::EXPECT_EQ_VECTORS(expectedRemovedIndices, filteredIndices);
        pcl::test::EXPECT_EQ_VECTORS(expectedFilteredIndices, *cropHullFilter.getRemovedIndices());
        cropHullFilter.setCropOutside(true);
      }

      pcl::PointCloud<pcl::PointXYZ> filteredCloud;
      cropHullFilter.filter(filteredCloud);
      ASSERT_EQ (expectedCloud.size(), filteredCloud.size());
      for (pcl::index_t i = 0; i < expectedCloud.size(); ++i)
      {
        EXPECT_XYZ_NEAR(expectedCloud[i], filteredCloud[i], 1e-5);
      }
      // check non empty out cloud filtering
      cropHullFilter.filter(filteredCloud);
      EXPECT_EQ (expectedCloud.size(), filteredCloud.size());

      // check keep organized
      {
        cropHullFilter.setKeepOrganized(true);
        cropHullFilter.setUserFilterValue(-10.);
        pcl::PointXYZ defaultPoint(-10., -10., -10.);
        cropHullFilter.filter(filteredCloud);
        ASSERT_EQ (inputCloud->size(), filteredCloud.size());
        for (pcl::index_t i = 0; i < inputCloud->size(); ++i)
        {
          pcl::PointXYZ expectedPoint = expectedFilteredMask[i] ? inputCloud->at(i) : defaultPoint;
          EXPECT_XYZ_NEAR(expectedPoint, filteredCloud[i], 1e-5);
        }
        cropHullFilter.setKeepOrganized(false);
      }
    }
  };

  bool randomBool() {
    static std::default_random_engine gen;
    static std::uniform_int_distribution<> int_distr(0, 1);
    return int_distr(gen);
  }

  struct TestData
  {
    TestData(pcl::Indices const & insideIndices, pcl::PointCloud<pcl::PointXYZ>::ConstPtr inputCloud)
      : inputCloud(inputCloud),
        insideMask(inputCloud->size(), false),
        insideIndices(insideIndices),
        insideCloud(new pcl::PointCloud<pcl::PointXYZ>),
        outsideCloud(new pcl::PointCloud<pcl::PointXYZ>)
    {
      pcl::copyPointCloud(*inputCloud, insideIndices, *insideCloud);
      for (pcl::index_t idx : insideIndices) {
        insideMask[idx] = true;
      }
      for (size_t i = 0; i < inputCloud->size(); ++i) {
        if (!insideMask[i]) {
          outsideIndices.push_back(i);
        }
      }
      pcl::copyPointCloud(*inputCloud, outsideIndices, *outsideCloud);
    }

    pcl::PointCloud<pcl::PointXYZ>::ConstPtr inputCloud;
    std::vector<bool> insideMask;
    pcl::Indices insideIndices, outsideIndices;
    pcl::PointCloud<pcl::PointXYZ>::Ptr insideCloud, outsideCloud;
  };


  std::vector<TestData>
  createTestDataSuite(
      std::function<pcl::PointXYZ()> insidePointGenerator,
      std::function<pcl::PointXYZ()> outsidePointGenerator)
  {
    std::vector<TestData> testDataSuite;
    size_t const chunkSize = 1000;
    pcl::PointCloud<pcl::PointXYZ>::Ptr insideCloud(new pcl::PointCloud<pcl::PointXYZ>);
    pcl::PointCloud<pcl::PointXYZ>::Ptr outsideCloud(new pcl::PointCloud<pcl::PointXYZ>);
    pcl::PointCloud<pcl::PointXYZ>::Ptr mixedCloud(new pcl::PointCloud<pcl::PointXYZ>);
    pcl::Indices insideIndicesForInsideCloud;
    pcl::Indices insideIndicesForOutsideCloud;
    pcl::Indices insideIndicesForMixedCloud;
    for (size_t i = 0; i < chunkSize; ++i)
    {
      insideIndicesForInsideCloud.push_back(i);
      insideCloud->push_back(insidePointGenerator());
      outsideCloud->push_back(outsidePointGenerator());
      if (randomBool()) {
        insideIndicesForMixedCloud.push_back(i);
        mixedCloud->push_back(insidePointGenerator());
      }
      else {
        mixedCloud->push_back(outsidePointGenerator());
      }
    }
    testDataSuite.emplace_back(insideIndicesForInsideCloud, insideCloud);
    testDataSuite.emplace_back(insideIndicesForOutsideCloud, outsideCloud);
    testDataSuite.emplace_back(insideIndicesForMixedCloud, mixedCloud);
    return testDataSuite;
  }


  class PCLCropHullTestFixtureBase : public ::testing::Test
  {
    public:
      PCLCropHullTestFixtureBase() {
        baseOffsetList.emplace_back(5, 1, 10);
        baseOffsetList.emplace_back(1, 5, 10);
        baseOffsetList.emplace_back(1, 10, 5);
        baseOffsetList.emplace_back(10, 1, 5);
        baseOffsetList.emplace_back(10, 5, 1);
      }
      static pcl::CropHull<pcl::PointXYZ> createDefaultCropHull (pcl::PointCloud<pcl::PointXYZ>::ConstPtr inputCloud) {
        pcl::CropHull<pcl::PointXYZ> cropHullFilter(true);
        pcl::ConvexHull<pcl::PointXYZ> convexHull;
        convexHull.setDimension(3);
        convexHull.setInputCloud(inputCloud);
        pcl::PointCloud<pcl::PointXYZ>::Ptr hullCloudPtr(new pcl::PointCloud<pcl::PointXYZ>);
        std::vector<pcl::Vertices> hullPolygons;
        convexHull.reconstruct(*hullCloudPtr, hullPolygons);
        cropHullFilter.setHullIndices(hullPolygons);
        cropHullFilter.setHullCloud(hullCloudPtr);
        return cropHullFilter;
      }
    protected:
      pcl::PointCloud<pcl::PointXYZ> baseOffsetList;
  };

  class PCLCropHullTestFixture2d : public PCLCropHullTestFixtureBase
  {
    public:
      PCLCropHullTestFixture2d()
        : gen(12345u),
          rd(0.0f, 1.0f),
          baseInputCloud(new pcl::PointCloud<pcl::PointXYZ>)
      {
        baseInputCloud->emplace_back(0.0f, 0.0f, 0.0f);
        baseInputCloud->emplace_back(0.0f, 1.0f, 0.0f);
        baseInputCloud->emplace_back(1.0f, 0.0f, 0.0f);
        baseInputCloud->emplace_back(1.0f, 1.0f, 0.0f);
        baseInputCloud->emplace_back(0.0f, 0.0f, 0.1f);
        baseInputCloud->emplace_back(0.0f, 1.0f, 0.1f);
        baseInputCloud->emplace_back(1.0f, 0.0f, 0.1f);
        baseInputCloud->emplace_back(1.0f, 1.0f, 0.1f);
      }
    protected:

      void test(std::function<void(pcl::CropHull<pcl::PointXYZ>, TestData const &)> checker) {
        pcl::PointCloud<pcl::PointXYZ>::Ptr inputCloud (new pcl::PointCloud<pcl::PointXYZ>);
        for (pcl::PointXYZ const & baseOffset : baseOffsetList)
        {
          pcl::copyPointCloud(*baseInputCloud, *inputCloud);
          for (pcl::PointXYZ & p : *inputCloud) {
            p.getVector3fMap() += baseOffset.getVector3fMap();
          }
          auto insidePointGenerator = [this, &baseOffset] () {
            pcl::PointXYZ p(rd(gen), rd(gen), 1.0);
            p.getVector3fMap() += baseOffset.getVector3fMap();
            return p;
          };
          auto outsidePointGenerator = [this, &baseOffset] () {
            pcl::PointXYZ p(rd(gen) + 2., rd(gen) + 2., rd(gen) + 2.);
            p.getVector3fMap() += baseOffset.getVector3fMap();
            return p;
          };
          pcl::CropHull<pcl::PointXYZ> cropHullFilter = createDefaultCropHull(inputCloud);
          cropHullFilter.setDim(2);
          std::vector<TestData> testDataSuite = createTestDataSuite(insidePointGenerator, outsidePointGenerator);
          for (TestData const & testData : testDataSuite)
          {
            checker(cropHullFilter, testData);
          }
        }
      }
    private:
      std::mt19937 gen;
      std::uniform_real_distribution<float> rd;
      pcl::PointCloud<pcl::PointXYZ>::Ptr baseInputCloud;
  };

  class PCLCropHullTestFixture3d : public PCLCropHullTestFixtureBase
  {
    public:
      PCLCropHullTestFixture3d()
        : gen(12345u),
          rd(0.0f, 1.0f),
          baseInputCloud(new pcl::PointCloud<pcl::PointXYZ>)
    {
      baseInputCloud->emplace_back(0.0f, 0.0f, 0.0f);
      baseInputCloud->emplace_back(0.0f, 0.0f, 1.0f);
      baseInputCloud->emplace_back(0.0f, 1.0f, 0.0f);
      baseInputCloud->emplace_back(0.0f, 1.0f, 1.0f);
      baseInputCloud->emplace_back(1.0f, 0.0f, 0.0f);
      baseInputCloud->emplace_back(1.0f, 0.0f, 1.0f);
      baseInputCloud->emplace_back(1.0f, 1.0f, 0.0f);
      baseInputCloud->emplace_back(1.0f, 1.0f, 1.0f);
    }
    protected:

      void test(std::function<void(pcl::CropHull<pcl::PointXYZ>, TestData const &)> checker) {
        pcl::PointCloud<pcl::PointXYZ>::Ptr inputCloud (new pcl::PointCloud<pcl::PointXYZ>);
        for (pcl::PointXYZ const & baseOffset : baseOffsetList)
        {
          pcl::copyPointCloud(*baseInputCloud, *inputCloud);
          for (pcl::PointXYZ & p : *inputCloud) {
            p.getVector3fMap() += baseOffset.getVector3fMap();
          }
          auto insidePointGenerator = [this, &baseOffset] () {
            pcl::PointXYZ p(rd(gen), rd(gen), rd(gen));
            p.getVector3fMap() += baseOffset.getVector3fMap();
            return p;
          };
          auto outsidePointGenerator = [this, &baseOffset] () {
            pcl::PointXYZ p(rd(gen) + 2., rd(gen) + 2., rd(gen) + 2.);
            p.getVector3fMap() += baseOffset.getVector3fMap();
            return p;
          };
          pcl::CropHull<pcl::PointXYZ> cropHullFilter = createDefaultCropHull(inputCloud);
          cropHullFilter.setDim(3);
          std::vector<TestData> testDataSuite = createTestDataSuite(insidePointGenerator, outsidePointGenerator);
          for (TestData const & testData : testDataSuite)
          {
            checker(cropHullFilter, testData);
          }
        }
      }
    private:
      std::mt19937 gen;
      std::uniform_real_distribution<float> rd;
      pcl::PointCloud<pcl::PointXYZ>::Ptr baseInputCloud;
  };
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
TEST_F (PCLCropHullTestFixture2d, 3dcude_simple_test)
{
  auto checker = [](pcl::CropHull<pcl::PointXYZ> cropHullFilter, TestData const & testData) {
    cropHullFilter.setInputCloud(testData.inputCloud);
    pcl::Indices filteredIndices;
    cropHullFilter.filter(filteredIndices);
    pcl::test::EXPECT_EQ_VECTORS(testData.insideIndices, filteredIndices);
  };
  test(checker);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
TEST_F (PCLCropHullTestFixture2d, 3dcude_test_reentrancy)
{
  auto checker = [](pcl::CropHull<pcl::PointXYZ> cropHullFilter, TestData const & testData) {
    cropHullFilter.setInputCloud(testData.inputCloud);
    pcl::Indices filteredIndices;
    cropHullFilter.filter(filteredIndices);
    pcl::test::EXPECT_EQ_VECTORS(testData.insideIndices, filteredIndices);
    cropHullFilter.filter(filteredIndices);
    pcl::test::EXPECT_EQ_VECTORS(testData.insideIndices, filteredIndices);
  };
  test(checker);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
TEST_F (PCLCropHullTestFixture2d, 3dcude_test_negative_filter_functionality)
{
  auto checker = [](pcl::CropHull<pcl::PointXYZ> cropHullFilter, TestData const & testData) {
    cropHullFilter.setInputCloud(testData.inputCloud);
    cropHullFilter.setNegative(true);
    pcl::Indices filteredIndices;
    cropHullFilter.filter(filteredIndices);
    pcl::test::EXPECT_EQ_VECTORS(testData.outsideIndices, filteredIndices);
    pcl::test::EXPECT_EQ_VECTORS(testData.insideIndices, *cropHullFilter.getRemovedIndices());
  };
  test(checker);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
TEST_F (PCLCropHullTestFixture2d, 3dcude_test_crop_inside)
{
  auto checker = [](pcl::CropHull<pcl::PointXYZ> cropHullFilter, TestData const & testData) {
    cropHullFilter.setInputCloud(testData.inputCloud);
    cropHullFilter.setCropOutside(false);
    pcl::Indices filteredIndices;
    cropHullFilter.filter(filteredIndices);
    pcl::test::EXPECT_EQ_VECTORS(testData.outsideIndices, filteredIndices);
    pcl::test::EXPECT_EQ_VECTORS(testData.insideIndices, *cropHullFilter.getRemovedIndices());
  };
  test(checker);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
TEST_F (PCLCropHullTestFixture2d, 3dcude_test_negative_crop_inside)
{
  auto checker = [](pcl::CropHull<pcl::PointXYZ> cropHullFilter, TestData const & testData) {
    cropHullFilter.setInputCloud(testData.inputCloud);
    cropHullFilter.setCropOutside(false);
    cropHullFilter.setNegative(true);
    pcl::Indices filteredIndices;
    cropHullFilter.filter(filteredIndices);
    pcl::test::EXPECT_EQ_VECTORS(testData.insideIndices, filteredIndices);
    pcl::test::EXPECT_EQ_VECTORS(testData.outsideIndices, *cropHullFilter.getRemovedIndices());
  };
  test(checker);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
TEST_F (PCLCropHullTestFixture2d, 3dcude_test_cloud_filtering)
{
  auto checker = [](pcl::CropHull<pcl::PointXYZ> cropHullFilter, TestData const & testData) {
    cropHullFilter.setInputCloud(testData.inputCloud);
    pcl::PointCloud<pcl::PointXYZ> filteredCloud;
    cropHullFilter.filter(filteredCloud);
    ASSERT_EQ (testData.insideCloud->size(), filteredCloud.size());
    for (pcl::index_t i = 0; i < testData.insideCloud->size(); ++i)
    {
      EXPECT_XYZ_NEAR(testData.insideCloud->at(i), filteredCloud[i], 1e-5);
    }
  };
  test(checker);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
TEST_F (PCLCropHullTestFixture2d, 3dcude_test_cloud_filtering_reentrancy)
{
  auto checker = [](pcl::CropHull<pcl::PointXYZ> cropHullFilter, TestData const & testData) {
    cropHullFilter.setInputCloud(testData.inputCloud);
    pcl::PointCloud<pcl::PointXYZ> filteredCloud;
    cropHullFilter.filter(filteredCloud);
    cropHullFilter.filter(filteredCloud);
    ASSERT_EQ (testData.insideCloud->size(), filteredCloud.size());
  };
  test(checker);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
TEST_F (PCLCropHullTestFixture2d, 3dcude_test_keep_organized)
{
  auto checker = [](pcl::CropHull<pcl::PointXYZ> cropHullFilter, TestData const & testData) {
    cropHullFilter.setInputCloud(testData.inputCloud);
    cropHullFilter.setKeepOrganized(true);
    cropHullFilter.setUserFilterValue(-10.);
    pcl::PointXYZ defaultPoint(-10., -10., -10.);
    pcl::PointCloud<pcl::PointXYZ> filteredCloud;
    cropHullFilter.filter(filteredCloud);
    ASSERT_EQ (testData.inputCloud->size(), filteredCloud.size());
    for (pcl::index_t i = 0; i < testData.inputCloud->size(); ++i)
    {
      pcl::PointXYZ expectedPoint = testData.insideMask[i] ? testData.inputCloud->at(i) : defaultPoint;
      EXPECT_XYZ_NEAR(expectedPoint, filteredCloud[i], 1e-5);
    }
  };
  test(checker);
}


//------------------ 3d CropHull version below ------------------------///


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
TEST_F (PCLCropHullTestFixture3d, 3dsquare_simple_test)
{
  auto checker = [](pcl::CropHull<pcl::PointXYZ> cropHullFilter, TestData const & testData) {
    cropHullFilter.setInputCloud(testData.inputCloud);
    pcl::Indices filteredIndices;
    cropHullFilter.filter(filteredIndices);
    pcl::test::EXPECT_EQ_VECTORS(testData.insideIndices, filteredIndices);
  };
  test(checker);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
TEST_F (PCLCropHullTestFixture3d, 3dsquare_test_reentrancy)
{
  auto checker = [](pcl::CropHull<pcl::PointXYZ> cropHullFilter, TestData const & testData) {
    cropHullFilter.setInputCloud(testData.inputCloud);
    pcl::Indices filteredIndices;
    cropHullFilter.filter(filteredIndices);
    pcl::test::EXPECT_EQ_VECTORS(testData.insideIndices, filteredIndices);
    cropHullFilter.filter(filteredIndices);
    pcl::test::EXPECT_EQ_VECTORS(testData.insideIndices, filteredIndices);
  };
  test(checker);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
TEST_F (PCLCropHullTestFixture3d, 3dsquare_test_negative_filter_functionality)
{
  auto checker = [](pcl::CropHull<pcl::PointXYZ> cropHullFilter, TestData const & testData) {
    cropHullFilter.setInputCloud(testData.inputCloud);
    cropHullFilter.setNegative(true);
    pcl::Indices filteredIndices;
    cropHullFilter.filter(filteredIndices);
    pcl::test::EXPECT_EQ_VECTORS(testData.outsideIndices, filteredIndices);
    pcl::test::EXPECT_EQ_VECTORS(testData.insideIndices, *cropHullFilter.getRemovedIndices());
  };
  test(checker);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
TEST_F (PCLCropHullTestFixture3d, 3dsquare_test_crop_inside)
{
  auto checker = [](pcl::CropHull<pcl::PointXYZ> cropHullFilter, TestData const & testData) {
    cropHullFilter.setInputCloud(testData.inputCloud);
    cropHullFilter.setCropOutside(false);
    pcl::Indices filteredIndices;
    cropHullFilter.filter(filteredIndices);
    pcl::test::EXPECT_EQ_VECTORS(testData.outsideIndices, filteredIndices);
    pcl::test::EXPECT_EQ_VECTORS(testData.insideIndices, *cropHullFilter.getRemovedIndices());
  };
  test(checker);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
TEST_F (PCLCropHullTestFixture3d, 3dsquare_test_negative_crop_inside)
{
  auto checker = [](pcl::CropHull<pcl::PointXYZ> cropHullFilter, TestData const & testData) {
    cropHullFilter.setInputCloud(testData.inputCloud);
    cropHullFilter.setCropOutside(false);
    cropHullFilter.setNegative(true);
    pcl::Indices filteredIndices;
    cropHullFilter.filter(filteredIndices);
    pcl::test::EXPECT_EQ_VECTORS(testData.insideIndices, filteredIndices);
    pcl::test::EXPECT_EQ_VECTORS(testData.outsideIndices, *cropHullFilter.getRemovedIndices());
  };
  test(checker);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
TEST_F (PCLCropHullTestFixture3d, 3dsquare_test_cloud_filtering)
{
  auto checker = [](pcl::CropHull<pcl::PointXYZ> cropHullFilter, TestData const & testData) {
    cropHullFilter.setInputCloud(testData.inputCloud);
    pcl::PointCloud<pcl::PointXYZ> filteredCloud;
    cropHullFilter.filter(filteredCloud);
    ASSERT_EQ (testData.insideCloud->size(), filteredCloud.size());
    for (pcl::index_t i = 0; i < testData.insideCloud->size(); ++i)
    {
      EXPECT_XYZ_NEAR(testData.insideCloud->at(i), filteredCloud[i], 1e-5);
    }
  };
  test(checker);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
TEST_F (PCLCropHullTestFixture3d, 3dsquare_test_cloud_filtering_reentrancy)
{
  auto checker = [](pcl::CropHull<pcl::PointXYZ> cropHullFilter, TestData const & testData) {
    cropHullFilter.setInputCloud(testData.inputCloud);
    pcl::PointCloud<pcl::PointXYZ> filteredCloud;
    cropHullFilter.filter(filteredCloud);
    cropHullFilter.filter(filteredCloud);
    ASSERT_EQ (testData.insideCloud->size(), filteredCloud.size());
  };
  test(checker);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
TEST_F (PCLCropHullTestFixture3d, 3dsquare_test_keep_organized)
{
  auto checker = [](pcl::CropHull<pcl::PointXYZ> cropHullFilter, TestData const & testData) {
    cropHullFilter.setInputCloud(testData.inputCloud);
    cropHullFilter.setKeepOrganized(true);
    cropHullFilter.setUserFilterValue(-10.);
    pcl::PointXYZ defaultPoint(-10., -10., -10.);
    pcl::PointCloud<pcl::PointXYZ> filteredCloud;
    cropHullFilter.filter(filteredCloud);
    ASSERT_EQ (testData.inputCloud->size(), filteredCloud.size());
    for (pcl::index_t i = 0; i < testData.inputCloud->size(); ++i)
    {
      pcl::PointXYZ expectedPoint = testData.insideMask[i] ? testData.inputCloud->at(i) : defaultPoint;
      EXPECT_XYZ_NEAR(expectedPoint, filteredCloud[i], 1e-5);
    }
  };
  test(checker);
}


/* ---[ */
int
main (int argc, char** argv)
{
  // Testing
  testing::InitGoogleTest (&argc, argv);
  return (RUN_ALL_TESTS ());
}
/* ]--- */
