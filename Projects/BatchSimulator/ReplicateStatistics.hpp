// Statistics for comparing replicate simulation runs (part of the batch runner, not the PAX library).
// Small-sample tests (Welch t, exact Mann-Whitney / permutation, Pearson / Spearman correlation) with
// Holm correction. All p-values are two-sided.

#ifndef PAXS_BATCH_SIMULATOR_REPLICATE_STATISTICS_HPP
#define PAXS_BATCH_SIMULATOR_REPLICATE_STATISTICS_HPP

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <utility>
#include <vector>

namespace paxs::stats {

    inline double nan() noexcept { return std::numeric_limits<double>::quiet_NaN(); }

    /// @brief Descriptive statistics
    struct Summary {
        std::size_t n = 0;
        double mean = nan();
        double sd = nan(); // sample standard deviation (n-1)
        double se = nan();
        double ci_low = nan(); // confidence interval of the mean (t distribution)
        double ci_high = nan();
        double median = nan();
        double min = nan();
        double max = nan();
    };

    /// @brief Test result
    struct TestResult {
        double statistic = nan();
        double df = nan();
        double p_value = nan();
        double effect_size = nan(); // Hedges' g, rank-biserial r, or correlation r
    };

    namespace detail {
        // Continued fraction for the regularised incomplete beta function (Numerical Recipes 6.4)
        inline double betaContinuedFraction(const double a, const double b, const double x) noexcept {
            constexpr int max_iterations = 300;
            constexpr double epsilon = 1e-15;
            constexpr double tiny = 1e-300;
            const double qab = a + b;
            const double qap = a + 1.0;
            const double qam = a - 1.0;
            double c = 1.0;
            double d = 1.0 - qab * x / qap;
            if (std::abs(d) < tiny) d = tiny;
            d = 1.0 / d;
            double h = d;
            for (int m = 1; m <= max_iterations; ++m) {
                const double m2 = 2.0 * m;
                double aa = m * (b - m) * x / ((qam + m2) * (a + m2));
                d = 1.0 + aa * d;
                if (std::abs(d) < tiny) d = tiny;
                c = 1.0 + aa / c;
                if (std::abs(c) < tiny) c = tiny;
                d = 1.0 / d;
                h *= d * c;
                aa = -(a + m) * (qab + m) * x / ((a + m2) * (qap + m2));
                d = 1.0 + aa * d;
                if (std::abs(d) < tiny) d = tiny;
                c = 1.0 + aa / c;
                if (std::abs(c) < tiny) c = tiny;
                d = 1.0 / d;
                const double delta = d * c;
                h *= delta;
                if (std::abs(delta - 1.0) < epsilon) break;
            }
            return h;
        }

        /// @brief Mid-ranks (ties get the average rank), 1-based
        inline std::vector<double> midRanks(const std::vector<double>& values) {
            const std::size_t n = values.size();
            std::vector<std::size_t> order(n);
            std::iota(order.begin(), order.end(), std::size_t{ 0 });
            std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) { return values[a] < values[b]; });
            std::vector<double> ranks(n);
            std::size_t i = 0;
            while (i < n) {
                std::size_t j = i;
                while (j + 1 < n && values[order[j + 1]] == values[order[i]]) ++j;
                const double rank = (static_cast<double>(i) + static_cast<double>(j)) / 2.0 + 1.0;
                for (std::size_t k = i; k <= j; ++k) ranks[order[k]] = rank;
                i = j + 1;
            }
            return ranks;
        }
    }

    /// @brief Regularised incomplete beta function I_x(a, b)
    inline double regularizedIncompleteBeta(const double a, const double b, const double x) noexcept {
        if (x <= 0.0) return 0.0;
        if (x >= 1.0) return 1.0;
        const double log_front = std::lgamma(a + b) - std::lgamma(a) - std::lgamma(b)
            + a * std::log(x) + b * std::log(1.0 - x);
        const double front = std::exp(log_front);
        if (x < (a + 1.0) / (a + b + 2.0)) {
            return front * detail::betaContinuedFraction(a, b, x) / a;
        }
        return 1.0 - front * detail::betaContinuedFraction(b, a, 1.0 - x) / b;
    }

    /// @brief Two-sided p-value of Student's t distribution
    inline double studentTTwoSidedP(const double t, const double df) noexcept {
        if (std::isnan(t) || std::isnan(df) || df <= 0.0) return nan();
        if (std::isinf(t)) return 0.0;
        return regularizedIncompleteBeta(df / 2.0, 0.5, df / (df + t * t));
    }

    /// @brief Quantile of Student's t distribution (probability = lower-tail probability)
    inline double studentTQuantile(const double probability, const double df) noexcept {
        if (!(probability > 0.0 && probability < 1.0) || !(df > 0.0)) return nan();
        if (probability == 0.5) return 0.0;
        const bool upper = probability > 0.5;
        const double two_sided = 2.0 * (upper ? 1.0 - probability : probability);
        double low = 0.0;
        double high = 1.0;
        while (studentTTwoSidedP(high, df) > two_sided) high *= 2.0;
        for (int i = 0; i < 200; ++i) {
            const double mid = (low + high) / 2.0;
            if (studentTTwoSidedP(mid, df) > two_sided) low = mid; else high = mid;
        }
        const double t = (low + high) / 2.0;
        return upper ? t : -t;
    }

    inline double mean(const std::vector<double>& x) noexcept {
        if (x.empty()) return nan();
        return std::accumulate(x.begin(), x.end(), 0.0) / static_cast<double>(x.size());
    }

    /// @brief Sample variance (n-1)
    inline double variance(const std::vector<double>& x) noexcept {
        if (x.size() < 2) return nan();
        const double m = mean(x);
        double sum = 0.0;
        for (const double v : x) sum += (v - m) * (v - m);
        return sum / static_cast<double>(x.size() - 1);
    }

    /// @brief Descriptive statistics with a t confidence interval of the mean
    inline Summary summarize(const std::vector<double>& x, const double confidence = 0.95) {
        Summary s;
        s.n = x.size();
        if (x.empty()) return s;
        std::vector<double> sorted = x;
        std::sort(sorted.begin(), sorted.end());
        s.mean = mean(x);
        s.min = sorted.front();
        s.max = sorted.back();
        const std::size_t mid = sorted.size() / 2;
        s.median = (sorted.size() % 2 == 1) ? sorted[mid] : (sorted[mid - 1] + sorted[mid]) / 2.0;
        if (x.size() >= 2) {
            s.sd = std::sqrt(variance(x));
            s.se = s.sd / std::sqrt(static_cast<double>(x.size()));
            const double t = studentTQuantile(0.5 + confidence / 2.0, static_cast<double>(x.size() - 1));
            s.ci_low = s.mean - t * s.se;
            s.ci_high = s.mean + t * s.se;
        }
        return s;
    }

    /// @brief Hedges' g: bias-corrected standardised mean difference (a - b)
    inline double hedgesG(const std::vector<double>& a, const std::vector<double>& b) noexcept {
        if (a.size() < 2 || b.size() < 2) return nan();
        const double na = static_cast<double>(a.size());
        const double nb = static_cast<double>(b.size());
        const double pooled = std::sqrt(((na - 1.0) * variance(a) + (nb - 1.0) * variance(b)) / (na + nb - 2.0));
        if (pooled == 0.0) return nan();
        const double correction = 1.0 - 3.0 / (4.0 * (na + nb) - 9.0);
        return (mean(a) - mean(b)) / pooled * correction;
    }

    /// @brief Welch's two-sample t-test (two-sided)
    /// @details If both groups have zero variance: p = 1 when the means are equal, otherwise p = 0.
    inline TestResult welchTTest(const std::vector<double>& a, const std::vector<double>& b) {
        TestResult r;
        if (a.size() < 2 || b.size() < 2) return r;
        const double va = variance(a) / static_cast<double>(a.size());
        const double vb = variance(b) / static_cast<double>(b.size());
        const double diff = mean(a) - mean(b);
        r.effect_size = hedgesG(a, b);
        if (va + vb == 0.0) {
            r.statistic = (diff == 0.0) ? 0.0 : std::copysign(std::numeric_limits<double>::infinity(), diff);
            r.p_value = (diff == 0.0) ? 1.0 : 0.0;
            return r;
        }
        r.statistic = diff / std::sqrt(va + vb);
        const double na = static_cast<double>(a.size());
        const double nb = static_cast<double>(b.size());
        r.df = (va + vb) * (va + vb) / (va * va / (na - 1.0) + vb * vb / (nb - 1.0));
        r.p_value = studentTTwoSidedP(r.statistic, r.df);
        return r;
    }

    /// @brief One-sample t-test against mu (two-sided)
    inline TestResult oneSampleTTest(const std::vector<double>& x, const double mu) {
        TestResult r;
        if (x.size() < 2) return r;
        const double sd = std::sqrt(variance(x));
        const double diff = mean(x) - mu;
        r.df = static_cast<double>(x.size() - 1);
        if (sd == 0.0) {
            r.statistic = (diff == 0.0) ? 0.0 : std::copysign(std::numeric_limits<double>::infinity(), diff);
            r.p_value = (diff == 0.0) ? 1.0 : 0.0;
            return r;
        }
        r.statistic = diff / (sd / std::sqrt(static_cast<double>(x.size())));
        r.p_value = studentTTwoSidedP(r.statistic, r.df);
        r.effect_size = diff / sd; // Cohen's d
        return r;
    }

    /// @brief Exact Mann-Whitney U test (permutation of mid-ranks over every split, n1 + n2 <= 24)
    /// @details Effect size is the rank-biserial correlation r = 2U/(n1 n2) - 1 (positive when a > b).
    inline TestResult mannWhitneyExact(const std::vector<double>& a, const std::vector<double>& b) {
        TestResult r;
        const std::size_t n1 = a.size();
        const std::size_t n2 = b.size();
        const std::size_t n = n1 + n2;
        if (n1 == 0 || n2 == 0 || n > 24) return r;

        std::vector<double> pooled(a);
        pooled.insert(pooled.end(), b.begin(), b.end());
        const std::vector<double> ranks = detail::midRanks(pooled);
        double observed_rank_sum = 0.0;
        for (std::size_t i = 0; i < n1; ++i) observed_rank_sum += ranks[i];
        const double expected_rank_sum = static_cast<double>(n1) * static_cast<double>(n + 1) / 2.0;
        const double observed_deviation = std::abs(observed_rank_sum - expected_rank_sum);

        const double u = observed_rank_sum - static_cast<double>(n1 * (n1 + 1)) / 2.0;
        r.statistic = u;
        r.effect_size = 2.0 * u / static_cast<double>(n1 * n2) - 1.0;

        std::uint_least64_t total = 0;
        std::uint_least64_t extreme = 0;
        std::vector<std::size_t> index(n1);
        std::iota(index.begin(), index.end(), std::size_t{ 0 });
        while (true) {
            double rank_sum = 0.0;
            for (const std::size_t i : index) rank_sum += ranks[i];
            ++total;
            if (std::abs(rank_sum - expected_rank_sum) >= observed_deviation - 1e-9) ++extreme;
            std::size_t k = n1;
            while (k > 0 && index[k - 1] == n - n1 + (k - 1)) --k;
            if (k == 0) break;
            ++index[k - 1];
            for (std::size_t j = k; j < n1; ++j) index[j] = index[j - 1] + 1;
        }
        r.p_value = static_cast<double>(extreme) / static_cast<double>(total);
        return r;
    }

    /// @brief Pearson correlation with t-test of r = 0
    inline TestResult pearsonCorrelation(const std::vector<double>& x, const std::vector<double>& y) {
        TestResult r;
        const std::size_t n = x.size();
        if (n != y.size() || n < 3) return r;
        const double mx = mean(x);
        const double my = mean(y);
        double sxy = 0.0, sxx = 0.0, syy = 0.0;
        for (std::size_t i = 0; i < n; ++i) {
            sxy += (x[i] - mx) * (y[i] - my);
            sxx += (x[i] - mx) * (x[i] - mx);
            syy += (y[i] - my) * (y[i] - my);
        }
        if (sxx == 0.0 || syy == 0.0) return r;
        const double rho = std::clamp(sxy / std::sqrt(sxx * syy), -1.0, 1.0);
        r.effect_size = rho;
        r.df = static_cast<double>(n - 2);
        r.statistic = (std::abs(rho) == 1.0)
            ? std::copysign(std::numeric_limits<double>::infinity(), rho)
            : rho * std::sqrt(r.df / (1.0 - rho * rho));
        r.p_value = studentTTwoSidedP(r.statistic, r.df);
        return r;
    }

    /// @brief Spearman rank correlation (t approximation)
    inline TestResult spearmanCorrelation(const std::vector<double>& x, const std::vector<double>& y) {
        if (x.size() != y.size()) return TestResult{};
        return pearsonCorrelation(detail::midRanks(x), detail::midRanks(y));
    }

    /// @brief Holm-adjusted p-values (NaN entries are left as NaN)
    inline std::vector<double> holmAdjust(const std::vector<double>& p_values) {
        std::vector<std::size_t> order;
        for (std::size_t i = 0; i < p_values.size(); ++i) {
            if (!std::isnan(p_values[i])) order.push_back(i);
        }
        std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) { return p_values[a] < p_values[b]; });
        std::vector<double> adjusted(p_values.size(), nan());
        const double m = static_cast<double>(order.size());
        double running_max = 0.0;
        for (std::size_t k = 0; k < order.size(); ++k) {
            const double value = std::min(1.0, (m - static_cast<double>(k)) * p_values[order[k]]);
            running_max = std::max(running_max, value);
            adjusted[order[k]] = running_max;
        }
        return adjusted;
    }

    /// @brief Significance stars
    inline const char* significanceStars(const double p) noexcept {
        if (std::isnan(p)) return "";
        if (p < 0.001) return "***";
        if (p < 0.01) return "**";
        if (p < 0.05) return "*";
        return "n.s.";
    }

}

#endif // !PAXS_BATCH_SIMULATOR_REPLICATE_STATISTICS_HPP
