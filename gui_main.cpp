// Entry point for the Qt interface. This is the block that is commented out at the
// end of main.cpp, moved here so the command-line batch and the GUI can both be built.
#include <QApplication>
#include "addressbook.h"

int main(int argc, char *argv[]) {
	QApplication app(argc, argv);

	AddressBook addressBook;

	addressBook.show();

	return app.exec();
}
