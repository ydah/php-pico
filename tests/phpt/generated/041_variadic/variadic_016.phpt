--TEST--
variadic argument collection vector 016
--FILE--
<?php
function generated_variadic_016($base, ...$values) {
    return array_reduce($values, fn($sum, $value) => $sum + $value, $base);
}
echo generated_variadic_016(33, 17, 18, 19, 20);
--EXPECT--
107
