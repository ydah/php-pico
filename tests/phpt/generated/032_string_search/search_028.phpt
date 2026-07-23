--TEST--
string search and slice vector 028
--FILE--
<?php
$text = 'iineedle28s';
echo strpos($text, 'needle28'), ':', (str_contains($text, 'needle28') ? 1 : 0), ':', (str_starts_with($text, 'ii') ? 1 : 0), ':', (str_ends_with($text, 's') ? 1 : 0), ':', substr($text, 2, 8);
--EXPECT--
2:1:1:1:needle28
