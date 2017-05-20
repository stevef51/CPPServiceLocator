#include <iostream>
#include <vector>
#include "ServiceLocator.hpp"

template <class T>
using sptr = std::shared_ptr<T>;

// Some plain interfaces
class IFood {
public:
	virtual std::string name() = 0;
};

class IAnimal {
public:
	virtual void eatFavouriteFood() = 0;
};


// Concrete classes which implement our interfaces, these 2 have no dependancies
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

// Monkey requires a favourite food, note it is not dependant on ServiceLocator now
class Monkey : public IAnimal {
private:
	sptr<IFood> _food;

public:
	Monkey(sptr<IFood> food) : _food(food) {
	}

	void eatFavouriteFood() override {
		std::cout << "Monkey eats " << _food->name() << "\n";
	}
};

// Human requires a favourite food, note it is not dependant on ServiceLocator now
class Human : public IAnimal {
private:
	sptr<IFood> _food;

public:
	Human(sptr<IFood> food) : _food(food) {
	}

	void eatFavouriteFood() override {
		std::cout << "Human eats " << _food->name() << "\n";
	}
};

/* The SLModule classes are ServiceLocator aware, and they are also intimate with the concrete classes they bind to
   and so know what dependancies are required to create instances */
class FoodSLModule : public ServiceLocator::Module {
public:
	void load() override {
		bind<IFood>("Monkey").to<Banana>([] (SLContext_sptr slc) { 
			return new Banana();
		});
		bind<IFood>("Human").to<Pizza>([] (SLContext_sptr slc) { 
			return new Pizza();
		});
	}
};

class AnimalsSLModule : public ServiceLocator::Module {
public:
	void load() override {
		bind<IAnimal>("Human").to<Human>([] (SLContext_sptr slc) { 
			return new Human(slc->resolve<IFood>("Human"));
		});
		bind<IAnimal>("Monkey").to<Monkey>([] (SLContext_sptr slc) { 
			return new Monkey(slc->resolve<IFood>("Monkey"));
		});
	}
};

int main(int argc, const char * argv[]) {
    auto sl = ServiceLocator::create();

    sl->modules()
    	.add<FoodSLModule>()
    	.add<AnimalsSLModule>();

    auto slc = sl->getContext();

    std::vector<sptr<IAnimal>> animals;
    slc->resolveAll<IAnimal>(&animals);

    for(auto animal : animals) {
    	animal->eatFavouriteFood();
    }

    return 0;
}
