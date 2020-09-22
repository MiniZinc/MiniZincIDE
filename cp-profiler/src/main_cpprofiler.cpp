#include <iostream>

#include <QApplication>

#include "cpprofiler/command_line_parser.hh"
#include "cpprofiler/conductor.hh"
#include "cpprofiler/options.hh"

#include "cpprofiler/tests/tree_test.hh"
#include "cpprofiler/tests/execution_test.hh"
#include "cpprofiler/utils/debug.hh"

int main(int argc, char *argv[])
{

    using namespace cpprofiler;

#ifdef QT_OPENGL_SUPPORT
    QGL::setPreferredPaintEngine(QPaintEngine::OpenGL);
#endif

    QApplication app(argc, argv);
    QCoreApplication::setApplicationName("CP-Profiler");

    CommandLineParser cl_parser;
    cl_parser.process(app);

    Options options;

    {
        if (cl_parser.isSet(cl_options::paths))
        {
            options.paths = cl_parser.value(cl_options::paths).toStdString();
            print("selected paths file: {}", options.paths);
        }

        if (cl_parser.isSet(cl_options::mzn))
        {
            options.mzn = cl_parser.value(cl_options::mzn).toStdString();
            print("selected mzn file: {}", options.mzn);
        }
    }

    if (cl_parser.isSet(cl_options::save_search))
    {
        const auto path = cl_parser.value(cl_options::save_search).toStdString();
        options.save_search_path = path;
    }

    if (cl_parser.isSet(cl_options::save_execution))
    {
        const auto path = cl_parser.value(cl_options::save_execution).toStdString();
        options.save_execution_db = path;
    }

    if(cl_parser.isSet(cl_options::save_pixel_tree)) {
        const auto path = cl_parser.value(cl_options::save_pixel_tree).toStdString();
        options.save_pixel_tree_path = path;
    }

    if(cl_parser.isSet(cl_options::pixel_tree_compression)) {
        QString cs = cl_parser.value(cl_options::pixel_tree_compression);
        options.pixel_tree_compression = cs.toInt();
    }

    Conductor conductor(std::move(options));

    conductor.show();

    tests::execution::run(conductor);

    return app.exec();
}

/// Threads
// Main thread
// Receiver thread
