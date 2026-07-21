<?php

function divide($left, $right)
{
    return $left / $right;
}

try {
    echo divide(12, 0);
} catch (DivisionByZeroError $error) {
    echo "caught: ", $error->getMessage(), "\n";
} finally {
    echo "cleanup complete\n";
}
