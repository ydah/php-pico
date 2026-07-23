--TEST--
string search and slice vector 009
--FILE--
<?php
$text = 'jneedle09tt';
echo strpos($text, 'needle09'), ':', (str_contains($text, 'needle09') ? 1 : 0), ':', (str_starts_with($text, 'j') ? 1 : 0), ':', (str_ends_with($text, 'tt') ? 1 : 0), ':', substr($text, 1, 8);
--EXPECT--
1:1:1:1:needle09
