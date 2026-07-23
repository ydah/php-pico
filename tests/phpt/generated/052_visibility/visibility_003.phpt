--TEST--
private and protected member access vector 003
--FILE--
<?php
class GeneratedVisibility003 {
    private $left = 25; protected $right = 21;
    private function left() { return $this->left; }
    protected function right() { return $this->right; }
    public function total() { return $this->left() + $this->right(); }
}
echo (new GeneratedVisibility003())->total();
--EXPECT--
46
