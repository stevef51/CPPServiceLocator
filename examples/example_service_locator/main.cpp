#include <iostream>
#include "ServiceLocator.hpp"

template <class T>
using sptr = std::shared_ptr<T>;

class IFood {
public:
	virtual std::string name() = 0;
};

class Banana : public IFood {
public:
	std::string name() override {
		return "Banana";
	}
};

class Pizza : public IFood {
public:
	std::string name() override {
		return "Pizza";
	}
};

class IAnimal {
public:
	virtual void eatFavouriteFood() = 0;
};

class Monkey : public IAnimal {
private:
	sptr<IFood> _food;

public:
	Monkey(SLContext_sptr slc) : _food(slc->resolve<IFood>("Monkey")) {
	}

	void eatFavouriteFood() override {
		std::cout << "Monkey eats " << _food->name() << "\n";
	}
};

class Human : public IAnimal {
private:
	sptr<IFood> _food;

public:
	Human(SLContext_sptr slc) : _food(slc->resolve<IFood>("Human")) {
	}

	void eatFavouriteFood() override {
		std::cout << "Human eats " << _food->name() << "\n";
	}
};

int main(int argc, const char * argv[]) {
    auto sl = ServiceLocator::create();
    sl->bind<IAnimal>("Monkey").to<Monkey>([] (SLContext_sptr slc) { return new Monkey(slc); });
    sl->bind<IAnimal>("Human").to<Human>([] (SLContext_sptr slc) { return new Human(slc); });
    sl->bind<IFood>("Monkey").toNoDependancy<Banana>();
    sl->bind<IFood>("Human").toNoDependancy<Pizza>();

    auto slc = sl->getContext();

    auto monkey = slc->resolve<IAnimal>("Monkey");
    monkey->eatFavouriteFood();

    auto human = slc->resolve<IAnimal>("Human");
    human->eatFavouriteFood();

    return 0;
}
