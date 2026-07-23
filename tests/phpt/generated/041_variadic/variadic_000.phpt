--TEST--
variadic argument collection vector 000
--FILE--
<?php
function generated_variadic_000($base, ...$values) {
    return array_reduce($values, fn($sum, $value) => $sum + $value, $base);
}
echo generated_variadic_000(1, 1, 2, 3);
--EXPECT--
7
