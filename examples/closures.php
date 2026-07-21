<?php

$offset = 10;
$add = function ($value) use ($offset) {
    return $value + $offset;
};
$multiply = fn($value) => $value * $offset;

$offset = 99;
echo $add(5), "\n";
echo $multiply(2), "\n";
