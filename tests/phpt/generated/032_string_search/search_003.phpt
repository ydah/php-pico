--TEST--
string search and slice vector 003
--FILE--
<?php
$text = 'dneedle03nnnn';
echo strpos($text, 'needle03'), ':', (str_contains($text, 'needle03') ? 1 : 0), ':', (str_starts_with($text, 'd') ? 1 : 0), ':', (str_ends_with($text, 'nnnn') ? 1 : 0), ':', substr($text, 1, 8);
--EXPECT--
1:1:1:1:needle03
