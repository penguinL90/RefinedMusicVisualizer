#pragma once

struct Vector2
{
    float X, Y;
    inline bool operator==(const Vector2&) const = default;
    inline Vector2& operator+=(const Vector2& a)
    {
        X += a.X;
        Y += a.Y;
        return *this;
    }
    inline Vector2& operator-=(const Vector2& a)
    {
        X -= a.X;
        Y -= a.Y;
        return *this;
    }
    inline Vector2& operator*=(const Vector2& a)
    {
        X *= a.X;
        Y *= a.Y;
        return *this;
    }
    inline Vector2& operator*=(const float val)
    {
        X *= val;
        Y *= val;
        return *this;
    }
    inline Vector2& operator/=(const Vector2& a)
    {
        X /= a.X;
        Y /= a.Y;
        return *this;
    }
    inline Vector2& operator/=(const float val)
    {
        X /= val;
        Y /= val;
        return *this;
    }

    inline Vector2& Normalize()
    {
        float length = Length();
        if (length == 0) return *this;
        X /= length;
        Y /= length;
        return *this;
    }

    inline Vector2 GetNorm() const
    {
        return { -Y, X };
    }

    inline float Length() const
    {
        return sqrtf(LengthSquared());
    }

    inline float LengthSquared() const
    {
        return X * X + Y * Y;
    }

    inline float Distance(Vector2 const& other) const
    {
        return sqrtf(powf(X - other.X, 2) + powf(Y - other.Y, 2));
    }

    static inline float Distance(Vector2 const& a, Vector2 const& b)
    {
        return a.Distance(b);
    }

    inline Vector2& Floor()
    {
        X = floorf(X);
        Y = floorf(Y);
        return *this;
    }

    inline Vector2& Ceil()
    {
        X = ceilf(X);
        Y = ceilf(Y);
        return *this;
    }
};

inline Vector2 operator+(const Vector2& a, const Vector2& b)
{
    return Vector2{ a.X + b.X, a.Y + b.Y };
}
inline Vector2 operator-(const Vector2& a, const Vector2& b)
{
    return Vector2{ a.X - b.X, a.Y - b.Y };
}
inline float operator*(const Vector2& a, const Vector2& b)
{
    return  a.X * b.X + a.Y * b.Y;
}
inline Vector2 operator*(const float val, const Vector2& b)
{
    return Vector2{ val * b.X, val * b.Y };
}
inline Vector2 operator*(const Vector2& a, const float val)
{
    return Vector2{ a.X * val, a.Y * val };
}
inline Vector2 operator/(const float val, const Vector2& b)
{
    return Vector2{ val / b.X, val / b.Y };
}
inline Vector2 operator/(const Vector2& a, const float val)
{
    return Vector2{ a.X / val, a.Y / val };
}

namespace std
{
    template<>
    struct hash<Vector2>
    {
        std::size_t operator()(const Vector2& v) const noexcept
        {
            std::size_t h1 = std::hash<float>{}(v.X);
            std::size_t h2 = std::hash<float>{}(v.Y);
            return h1 ^ (h2 << 1);
        }
    };
}

