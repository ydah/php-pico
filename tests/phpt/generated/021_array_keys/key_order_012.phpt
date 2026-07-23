--TEST--
associative key order vector 012
--FILE--
<?php
$items = ['left' => 22, 4 => 56, 'right' => 90];
unset($items[4]);
$items['tail'] = 91;
$keys = array_keys($items); $values = array_values($items);
echo implode(',', $keys), ':', implode(',', $values), ':', (array_key_exists('right', $items) ? 1 : 0);
--EXPECT--
left,right,tail:22,90,91:1
