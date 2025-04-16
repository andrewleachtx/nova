#pragma once

/*
    A collection of derived classes each storing a given contribution function as well as 
    the necessary attributes to execute that function. The general idea is to use polymorphism
    to allow for function selection and setup at runtime.

    This will allow for a standard method to add new contribution methods and a localized place for 
    setting attributes such as ignore_polarity.
*/
static float contribution = 0.15f;

class BaseFunc {
    public:
        BaseFunc() {};
        virtual ~BaseFunc() = default;

        void setX(float val) { x = val; };
        void setY(float val) { y = val; };
        void setT(float val) { t = val; };
        void setPolarity(float val) { polarity = val == 0 ? -1 : 1; };

        virtual float getWeight() const { return contribution * polarity; }; // Consider including polarity

    protected:
        float x;
        float y;
        float t;
        float polarity;

};

class morletFunc: public BaseFunc {
    public:
        morletFunc(float f, float h, float center_t): BaseFunc(), f(f), h(h), center_t(center_t) {};
        ~morletFunc() override = default;

        float getWeight() const override { 
            auto complex_result = std::exp(2.0f * std::complex<float>(0.0f, 1.0f) * std::acos(-1.0f) * f * (t - center_t)) * 
                (float) std::exp(-4 * std::log(2) * std::pow((t - center_t), 2) / std::pow(h, 2));
            return std::real(complex_result) * polarity * contribution;         
        };

    private:
        float f;
        float h;
        float center_t;
};

    