
#include "command_line_parser.hh"

namespace cpprofiler
{

namespace cl_options
{
QCommandLineOption paths{"paths", "Use symbol table from: <file_name>.", "file_name"};
QCommandLineOption mzn{"mzn", "Use MiniZinc file for tying ids to expressions: <file_name>.", "file_name"};
QCommandLineOption save_search{"save_search", "Process one execution and save its search to <file_name>; terminate afterwards.", "file_name"};
QCommandLineOption save_execution{"save_execution", "Process one execution and save it a database named <file_name>; terminate afterwards.", "file_name"};
QCommandLineOption save_pixel_tree{"save_pixel_tree", "Process one execution and save it a database named <file_name>; terminate afterwards.", "file_name"};
QCommandLineOption pixel_tree_compression{"pixel_tree_compression", "What compression factor to use for saved pixel tree. Default: 2", "2"};
} // namespace cl_options

CommandLineParser::CommandLineParser()
{

    cl_parser.addHelpOption();
    cl_parser.addOption(cl_options::paths);
    cl_parser.addOption(cl_options::mzn);
    cl_parser.addOption(cl_options::save_search);
    cl_parser.addOption(cl_options::save_execution);
    cl_parser.addOption(cl_options::save_pixel_tree);
    cl_parser.addOption(cl_options::pixel_tree_compression);
}

void CommandLineParser::process(const QCoreApplication &app)
{
    cl_parser.process(app);
}

QString CommandLineParser::value(const QCommandLineOption &opt)
{
    return cl_parser.value(opt);
}

bool CommandLineParser::isSet(const QCommandLineOption &opt)
{
    return cl_parser.isSet(opt);
}
} // namespace cpprofiler
