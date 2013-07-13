#include "precomp.hpp"


///////////////////////////////////////////////////////////////////////////////////////////////
/// Point Cloud Widget implementation

struct cv::viz::CloudWidget::CreateCloudWidget
{
    static inline vtkSmartPointer<vtkPolyData> create(const Mat &cloud, vtkIdType &nr_points)
    {
        vtkSmartPointer<vtkPolyData> polydata = vtkSmartPointer<vtkPolyData>::New ();
        vtkSmartPointer<vtkCellArray> vertices = vtkSmartPointer<vtkCellArray>::New ();

        polydata->SetVerts (vertices);

        vtkSmartPointer<vtkPoints> points = polydata->GetPoints();
        vtkSmartPointer<vtkIdTypeArray> initcells;
        nr_points = cloud.total();

        if (!points)
        {
            points = vtkSmartPointer<vtkPoints>::New ();
            if (cloud.depth() == CV_32F)
                points->SetDataTypeToFloat();
            else if (cloud.depth() == CV_64F)
                points->SetDataTypeToDouble();
            polydata->SetPoints (points);
        }
        points->SetNumberOfPoints (nr_points);

        if (cloud.depth() == CV_32F)
        {
            // Get a pointer to the beginning of the data array
            Vec3f *data_beg = vtkpoints_data<float>(points);
            Vec3f *data_end = NanFilter::copy(cloud, data_beg, cloud);
            nr_points = data_end - data_beg;
        }
        else if (cloud.depth() == CV_64F)
        {
            // Get a pointer to the beginning of the data array
            Vec3d *data_beg = vtkpoints_data<double>(points);
            Vec3d *data_end = NanFilter::copy(cloud, data_beg, cloud);
            nr_points = data_end - data_beg;
        }
        points->SetNumberOfPoints (nr_points);

        // Update cells
        vtkSmartPointer<vtkIdTypeArray> cells = vertices->GetData ();
        // If no init cells and cells has not been initialized...
        if (!cells)
            cells = vtkSmartPointer<vtkIdTypeArray>::New ();

        // If we have less values then we need to recreate the array
        if (cells->GetNumberOfTuples () < nr_points)
        {
            cells = vtkSmartPointer<vtkIdTypeArray>::New ();

            // If init cells is given, and there's enough data in it, use it
            if (initcells && initcells->GetNumberOfTuples () >= nr_points)
            {
                cells->DeepCopy (initcells);
                cells->SetNumberOfComponents (2);
                cells->SetNumberOfTuples (nr_points);
            }
            else
            {
                // If the number of tuples is still too small, we need to recreate the array
                cells->SetNumberOfComponents (2);
                cells->SetNumberOfTuples (nr_points);
                vtkIdType *cell = cells->GetPointer (0);
                // Fill it with 1s
                std::fill_n (cell, nr_points * 2, 1);
                cell++;
                for (vtkIdType i = 0; i < nr_points; ++i, cell += 2)
                    *cell = i;
                // Save the results in initcells
                initcells = vtkSmartPointer<vtkIdTypeArray>::New ();
                initcells->DeepCopy (cells);
            }
        }
        else
        {
            // The assumption here is that the current set of cells has more data than needed
            cells->SetNumberOfComponents (2);
            cells->SetNumberOfTuples (nr_points);
        }

        // Set the cells and the vertices
        vertices->SetCells (nr_points, cells);
        return polydata;
    }
};

cv::viz::CloudWidget::CloudWidget(InputArray _cloud, InputArray _colors)
{
    Mat cloud = _cloud.getMat();
    Mat colors = _colors.getMat();
    CV_Assert(cloud.type() == CV_32FC3 || cloud.type() == CV_64FC3 || cloud.type() == CV_32FC4 || cloud.type() == CV_64FC4);
    CV_Assert(colors.type() == CV_8UC3 && cloud.size() == colors.size());

    if (cloud.isContinuous() && colors.isContinuous())
    {
        cloud.reshape(cloud.channels(), 1);
        colors.reshape(colors.channels(), 1);
    }

    vtkIdType nr_points;
    vtkSmartPointer<vtkPolyData> polydata = CreateCloudWidget::create(cloud, nr_points);

    // Filter colors
    Vec3b* colors_data = new Vec3b[nr_points];
    NanFilter::copy(colors, colors_data, cloud);

    vtkSmartPointer<vtkUnsignedCharArray> scalars = vtkSmartPointer<vtkUnsignedCharArray>::New ();
    scalars->SetNumberOfComponents (3);
    scalars->SetNumberOfTuples (nr_points);
    scalars->SetArray (colors_data->val, 3 * nr_points, 0);

    // Assign the colors
    polydata->GetPointData ()->SetScalars (scalars);

    vtkSmartPointer<vtkDataSetMapper> mapper = vtkSmartPointer<vtkDataSetMapper>::New ();
    mapper->SetInput (polydata);

    Vec3d minmax(scalars->GetRange());
    mapper->SetScalarRange(minmax.val);
    mapper->SetScalarModeToUsePointData ();

    bool interpolation = (polydata && polydata->GetNumberOfCells () != polydata->GetNumberOfVerts ());

    mapper->SetInterpolateScalarsBeforeMapping (interpolation);
    mapper->ScalarVisibilityOn ();

    mapper->ImmediateModeRenderingOff ();

    vtkSmartPointer<vtkLODActor> actor = vtkSmartPointer<vtkLODActor>::New();
    actor->SetNumberOfCloudPoints (int (std::max<vtkIdType> (1, polydata->GetNumberOfPoints () / 10)));
    actor->GetProperty ()->SetInterpolationToFlat ();
    actor->GetProperty ()->BackfaceCullingOn ();
    actor->SetMapper (mapper);

    WidgetAccessor::setProp(*this, actor);
}

cv::viz::CloudWidget::CloudWidget(InputArray _cloud, const Color &color)
{
    Mat cloud = _cloud.getMat();
    CV_Assert(cloud.type() == CV_32FC3 || cloud.type() == CV_64FC3 || cloud.type() == CV_32FC4 || cloud.type() == CV_64FC4);


    vtkIdType nr_points;
    vtkSmartPointer<vtkPolyData> polydata = CreateCloudWidget::create(cloud, nr_points);

    vtkSmartPointer<vtkDataSetMapper> mapper = vtkSmartPointer<vtkDataSetMapper>::New ();
    mapper->SetInput (polydata);

    bool interpolation = (polydata && polydata->GetNumberOfCells () != polydata->GetNumberOfVerts ());

    mapper->SetInterpolateScalarsBeforeMapping (interpolation);
    mapper->ScalarVisibilityOff ();

    mapper->ImmediateModeRenderingOff ();

    vtkSmartPointer<vtkLODActor> actor = vtkSmartPointer<vtkLODActor>::New();
    actor->SetNumberOfCloudPoints (int (std::max<vtkIdType> (1, polydata->GetNumberOfPoints () / 10)));
    actor->GetProperty ()->SetInterpolationToFlat ();
    actor->GetProperty ()->BackfaceCullingOn ();
    actor->SetMapper (mapper);

    WidgetAccessor::setProp(*this, actor);
    setColor(color);
}

template<> cv::viz::CloudWidget cv::viz::Widget::cast<cv::viz::CloudWidget>()
{
    Widget3D widget = this->cast<Widget3D>();
    return static_cast<CloudWidget&>(widget);
}

///////////////////////////////////////////////////////////////////////////////////////////////
/// Cloud Normals Widget implementation

struct cv::viz::CloudNormalsWidget::ApplyCloudNormals
{
    template<typename _Tp>
    struct Impl
    {
        static vtkSmartPointer<vtkCellArray> applyOrganized(const Mat &cloud, const Mat& normals, double level, float scale, _Tp *&pts, vtkIdType &nr_normals)
        {
            vtkIdType point_step = static_cast<vtkIdType>(std::sqrt(level));
            nr_normals = (static_cast<vtkIdType> ((cloud.cols - 1) / point_step) + 1) *
                         (static_cast<vtkIdType> ((cloud.rows - 1) / point_step) + 1);
            vtkSmartPointer<vtkCellArray> lines = vtkSmartPointer<vtkCellArray>::New();

            pts = new _Tp[2 * nr_normals * 3];

            int cch = cloud.channels();
            vtkIdType cell_count = 0;
            for (vtkIdType y = 0; y < cloud.rows; y += point_step)
            {
                const _Tp *prow = cloud.ptr<_Tp>(y);
                const _Tp *nrow = normals.ptr<_Tp>(y);
                for (vtkIdType x = 0; x < cloud.cols; x += point_step * cch)
                {
                    pts[2 * cell_count * 3 + 0] = prow[x];
                    pts[2 * cell_count * 3 + 1] = prow[x+1];
                    pts[2 * cell_count * 3 + 2] = prow[x+2];
                    pts[2 * cell_count * 3 + 3] = prow[x] + nrow[x] * scale;
                    pts[2 * cell_count * 3 + 4] = prow[x+1] + nrow[x+1] * scale;
                    pts[2 * cell_count * 3 + 5] = prow[x+2] + nrow[x+2] * scale;

                    lines->InsertNextCell (2);
                    lines->InsertCellPoint (2 * cell_count);
                    lines->InsertCellPoint (2 * cell_count + 1);
                    cell_count++;
                }
            }
            return lines;
        }

        static vtkSmartPointer<vtkCellArray> applyUnorganized(const Mat &cloud, const Mat& normals, int level, float scale, _Tp *&pts, vtkIdType &nr_normals)
        {
            vtkSmartPointer<vtkCellArray> lines = vtkSmartPointer<vtkCellArray>::New();
            nr_normals = (cloud.size().area() - 1) / level + 1 ;
            pts = new _Tp[2 * nr_normals * 3];

            int cch = cloud.channels();
            const _Tp *p = cloud.ptr<_Tp>();
            const _Tp *n = normals.ptr<_Tp>();
            for (vtkIdType i = 0, j = 0; j < nr_normals; j++, i = j * level * cch)
            {

                pts[2 * j * 3 + 0] = p[i];
                pts[2 * j * 3 + 1] = p[i+1];
                pts[2 * j * 3 + 2] = p[i+2];
                pts[2 * j * 3 + 3] = p[i] + n[i] * scale;
                pts[2 * j * 3 + 4] = p[i+1] + n[i+1] * scale;
                pts[2 * j * 3 + 5] = p[i+2] + n[i+2] * scale;

                lines->InsertNextCell (2);
                lines->InsertCellPoint (2 * j);
                lines->InsertCellPoint (2 * j + 1);
            }
            return lines;
        }
    };

    template<typename _Tp>
    static inline vtkSmartPointer<vtkCellArray> apply(const Mat &cloud, const Mat& normals, int level, float scale, _Tp *&pts, vtkIdType &nr_normals)
    {
        if (cloud.cols > 1 && cloud.rows > 1)
            return ApplyCloudNormals::Impl<_Tp>::applyOrganized(cloud, normals, level, scale, pts, nr_normals);
        else
            return ApplyCloudNormals::Impl<_Tp>::applyUnorganized(cloud, normals, level, scale, pts, nr_normals);
    }
};

cv::viz::CloudNormalsWidget::CloudNormalsWidget(InputArray _cloud, InputArray _normals, int level, float scale, const Color &color)
{
    Mat cloud = _cloud.getMat();
    Mat normals = _normals.getMat();
    CV_Assert(cloud.type() == CV_32FC3 || cloud.type() == CV_64FC3 || cloud.type() == CV_32FC4 || cloud.type() == CV_64FC4);
    CV_Assert(cloud.size() == normals.size() && cloud.type() == normals.type());

    vtkSmartPointer<vtkPoints> points = vtkSmartPointer<vtkPoints>::New();
    vtkSmartPointer<vtkCellArray> lines = vtkSmartPointer<vtkCellArray>::New();
    vtkIdType nr_normals = 0;

    if (cloud.depth() == CV_32F)
    {
        points->SetDataTypeToFloat();

        vtkSmartPointer<vtkFloatArray> data = vtkSmartPointer<vtkFloatArray>::New ();
        data->SetNumberOfComponents (3);

        float* pts = 0;
        lines = ApplyCloudNormals::apply(cloud, normals, level, scale, pts, nr_normals);
        data->SetArray (&pts[0], 2 * nr_normals * 3, 0);
        points->SetData (data);
    }
    else
    {
        points->SetDataTypeToDouble();

        vtkSmartPointer<vtkDoubleArray> data = vtkSmartPointer<vtkDoubleArray>::New ();
        data->SetNumberOfComponents (3);

        double* pts = 0;
        lines = ApplyCloudNormals::apply(cloud, normals, level, scale, pts, nr_normals);
        data->SetArray (&pts[0], 2 * nr_normals * 3, 0);
        points->SetData (data);
    }

    vtkSmartPointer<vtkPolyData> polyData = vtkSmartPointer<vtkPolyData>::New();
    polyData->SetPoints (points);
    polyData->SetLines (lines);

    vtkSmartPointer<vtkDataSetMapper> mapper = vtkSmartPointer<vtkDataSetMapper>::New ();
    mapper->SetInput (polyData);
    mapper->SetColorModeToMapScalars();
    mapper->SetScalarModeToUsePointData();

    vtkSmartPointer<vtkLODActor> actor = vtkSmartPointer<vtkLODActor>::New();
    actor->SetMapper(mapper);
    WidgetAccessor::setProp(*this, actor);
    setColor(color);
}

template<> cv::viz::CloudNormalsWidget cv::viz::Widget::cast<cv::viz::CloudNormalsWidget>()
{
    Widget3D widget = this->cast<Widget3D>();
    return static_cast<CloudNormalsWidget&>(widget);
}
