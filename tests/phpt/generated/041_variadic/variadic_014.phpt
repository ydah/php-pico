--TEST--
variadic argument collection vector 014
--FILE--
<?php
function generated_variadic_014($base, ...$values) {
    return array_reduce($values, fn($sum, $value) => $sum + $value, $base);
}
echo generated_variadic_014(29, 15, 16, 17, 18, 19, 20, 21);
--EXPECT--
155
